#include "service/stream_service_protocol.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

struct Options {
    int delayMs = 0;
    bool malformed = false;
    bool oversizedLine = false;
    bool oversizedNoNewline = false;
    int lineBytes = 0;
    int stderrBytes = 0;
    bool stderrContinuous = false;
    bool stderrOnExit = false;
    bool wrongSearchShape = false;
    bool wrongPingOperation = false;
    bool matchingPingOperation = false;
    bool ignoreCancel = false;
    bool ignoreShutdown = false;
    int crashAfter = 0;
    QString crashOnceFile;
    int offlineAfter = 0;
    QString errorCode;
    QVector<bool> searchScript;
};

Options parseOptions(const QStringList &arguments)
{
    Options options;
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("--delay-ms") && index + 1 < arguments.size()) {
            options.delayMs = qMax(0, arguments.at(++index).toInt());
        } else if (argument == QStringLiteral("--malformed")) {
            options.malformed = true;
        } else if (argument == QStringLiteral("--oversized-line")) {
            options.oversizedLine = true;
        } else if (argument == QStringLiteral("--oversized-no-newline")) {
            options.oversizedNoNewline = true;
        } else if (argument == QStringLiteral("--line-bytes") && index + 1 < arguments.size()) {
            options.lineBytes = qMax(0, arguments.at(++index).toInt());
        } else if (argument == QStringLiteral("--stderr-bytes") && index + 1 < arguments.size()) {
            options.stderrBytes = qMax(0, arguments.at(++index).toInt());
        } else if (argument == QStringLiteral("--stderr-continuous")) {
            options.stderrContinuous = true;
        } else if (argument == QStringLiteral("--stderr-on-exit")) {
            options.stderrOnExit = true;
        } else if (argument == QStringLiteral("--wrong-search-shape")) {
            options.wrongSearchShape = true;
        } else if (argument == QStringLiteral("--wrong-ping-operation")) {
            options.wrongPingOperation = true;
        } else if (argument == QStringLiteral("--matching-ping-operation")) {
            options.matchingPingOperation = true;
        } else if (argument == QStringLiteral("--ignore-cancel")) {
            options.ignoreCancel = true;
        } else if (argument == QStringLiteral("--ignore-shutdown")) {
            options.ignoreShutdown = true;
        } else if (argument == QStringLiteral("--crash-after") && index + 1 < arguments.size()) {
            options.crashAfter = qMax(0, arguments.at(++index).toInt());
        } else if (argument == QStringLiteral("--crash-once-file") && index + 1 < arguments.size()) {
            options.crashOnceFile = arguments.at(++index);
        } else if (argument == QStringLiteral("--offline-after") && index + 1 < arguments.size()) {
            options.offlineAfter = qMax(0, arguments.at(++index).toInt());
        } else if (argument == QStringLiteral("--error-code") && index + 1 < arguments.size()) {
            options.errorCode = arguments.at(++index);
        } else if (argument == QStringLiteral("--search-script") && index + 1 < arguments.size()) {
            const QStringList states = arguments.at(++index).split(',', Qt::SkipEmptyParts);
            for (const QString &state : states) {
                if (state == QStringLiteral("online")) {
                    options.searchScript.push_back(true);
                } else if (state == QStringLiteral("offline")) {
                    options.searchScript.push_back(false);
                }
            }
        }
    }
    return options;
}

void emitObject(const QJsonObject &object)
{
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact);
    std::cout << line.constData() << '\n' << std::flush;
}

void emitError(quint64 requestId, const QString &code)
{
    QJsonObject error;
    error.insert(QStringLiteral("code"), code);
    error.insert(QStringLiteral("retryable"), code == QStringLiteral("TIMEOUT")
                                                 || code == QStringLiteral("SERVICE_FAILED"));
    QJsonObject response;
    response.insert(QStringLiteral("requestId"), static_cast<qint64>(requestId));
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"), error);
    emitObject(response);
}

void emitStderr(int bytes)
{
    QByteArray chunk(64 * 1024, 'x');
    chunk[0] = '\xff';
    while (bytes > 0) {
        const size_t count = static_cast<size_t>(qMin(bytes, chunk.size()));
        if (std::fwrite(chunk.constData(), 1, count, stderr) != count) break;
        bytes -= static_cast<int>(count);
    }
    std::fflush(stderr);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const Options options = parseOptions(application.arguments());
    std::atomic_bool inputClosed = false;
    std::mutex lineMutex;
    QList<QByteArray> lines;

    std::thread reader([&] {
        std::string line;
        while (std::getline(std::cin, line)) {
            const QByteArray value = QByteArray::fromStdString(line);
            QMetaObject::invokeMethod(&application,
                                      [&lines, &lineMutex, value] {
                                          const std::lock_guard lock(lineMutex);
                                          lines.push_back(value);
                                      },
                                      Qt::QueuedConnection);
        }
        inputClosed.store(true);
        QMetaObject::invokeMethod(&application, &QCoreApplication::quit, Qt::QueuedConnection);
    });

    QHash<quint64, QTimer *> pending;
    QTimer stderrTimer;
    if (options.stderrContinuous) {
        QObject::connect(&stderrTimer, &QTimer::timeout, &application,
                         [] { emitStderr(64 * 1024); });
    }
    int requestCount = 0;
    int resolveCount = 0;
    int searchCount = 0;
    QTimer poller;
    QObject::connect(&poller, &QTimer::timeout, &application, [&] {
        QList<QByteArray> current;
        {
            const std::lock_guard lock(lineMutex);
            current.swap(lines);
        }

        for (const QByteArray &line : current) {
            const auto request = decodeRequest(line);
            if (!request.has_value()) {
                emitError(0, QStringLiteral("INVALID_INPUT"));
                continue;
            }
            ++requestCount;
            if (options.crashAfter > 0 && requestCount >= options.crashAfter) {
                if (options.crashOnceFile.isEmpty() || !QFile::exists(options.crashOnceFile)) {
                    if (!options.crashOnceFile.isEmpty()) {
                        QFile marker(options.crashOnceFile);
                        marker.open(QIODevice::WriteOnly);
                    }
                    if (options.stderrBytes > 0) emitStderr(options.stderrBytes);
                    std::_Exit(17);
                }
            }
            if (options.stderrBytes > 0) emitStderr(options.stderrBytes);
            if (options.malformed) {
                std::cout << "not-json\n" << std::flush;
                continue;
            }

            if (options.stderrContinuous && !stderrTimer.isActive()) stderrTimer.start(1);
            if (options.stderrContinuous) {
                if (request->operation == ServiceOperation::Shutdown) application.quit();
                continue;
            }

            if (options.oversizedLine || options.oversizedNoNewline || options.lineBytes > 0) {
                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                response.insert(QStringLiteral("pong"), true);
                int targetBytes = options.lineBytes;
                if (options.oversizedLine || options.oversizedNoNewline) targetBytes = 1'100'000;
                response.insert(QStringLiteral("padding"), QString());
                const QByteArray base = QJsonDocument(response).toJson(QJsonDocument::Compact);
                response.insert(QStringLiteral("padding"),
                                QString(qMax(0, targetBytes - base.size()), QLatin1Char('x')));
                const QByteArray line = QJsonDocument(response).toJson(QJsonDocument::Compact);
                std::cout.write(line.constData(), line.size());
                if (options.oversizedLine) std::cout.put('\n');
                std::cout.flush();
                continue;
            }

            if (request->operation == ServiceOperation::Ping) {
                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                response.insert(QStringLiteral("pong"), true);
                if (options.wrongPingOperation) {
                    response.insert(QStringLiteral("op"), QStringLiteral("search"));
                } else if (options.matchingPingOperation) {
                    response.insert(QStringLiteral("op"), QStringLiteral("ping"));
                }
                emitObject(response);
            } else if (request->operation == ServiceOperation::Shutdown) {
                if (options.ignoreShutdown) continue;
                if (options.stderrOnExit) emitStderr(1024 * 1024);
                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                response.insert(QStringLiteral("shutdown"), true);
                emitObject(response);
                application.quit();
            } else if (request->operation == ServiceOperation::Cancel) {
                if (!options.ignoreCancel) {
                    if (auto timer = pending.take(request->targetRequestId)) {
                        timer->stop();
                        timer->deleteLater();
                    }
                }
                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                response.insert(QStringLiteral("cancelled"), static_cast<qint64>(request->targetRequestId));
                emitObject(response);
            } else if (request->operation == ServiceOperation::Resolve) {
                const int resolveIndex = ++resolveCount;
                auto *timer = new QTimer(&application);
                timer->setSingleShot(true);
                pending.insert(request->requestId, timer);
                QObject::connect(timer, &QTimer::timeout, &application,
                                 [&, timer, requestId = request->requestId, roomId = request->roomId,
                                  qualityRate = request->qualityRate, resolveIndex] {
                                     pending.remove(requestId);
                                     timer->deleteLater();
                                     if (!options.errorCode.isEmpty()) {
                                         emitError(requestId, options.errorCode);
                                         return;
                                     }
                                     if (options.offlineAfter > 0
                                         && resolveIndex == options.offlineAfter) {
                                         QJsonObject response;
                                         response.insert(QStringLiteral("requestId"),
                                                         static_cast<qint64>(requestId));
                                         response.insert(QStringLiteral("ok"), true);
                                         response.insert(QStringLiteral("roomId"), roomId);
                                         response.insert(QStringLiteral("isLive"), false);
                                         response.insert(QStringLiteral("variants"), QJsonArray());
                                         emitObject(response);
                                         return;
                                     }
                                     QJsonObject variant;
                                     variant.insert(QStringLiteral("id"), QStringLiteral("flv-auto"));
                                     variant.insert(QStringLiteral("label"),
                                                    qualityRate >= 0
                                                        ? QStringLiteral("rate-%1").arg(qualityRate)
                                                        : QStringLiteral("fake"));
                                     variant.insert(QStringLiteral("quality"), QStringLiteral("auto"));
                                     variant.insert(QStringLiteral("qualityRate"),
                                                    qualityRate >= 0
                                                        ? qualityRate : 4);
                                     variant.insert(QStringLiteral("container"), QStringLiteral("flv"));
                                     variant.insert(QStringLiteral("playbackUrl"),
                                                   QStringLiteral("https://live.douyucdn.cn/fake.flv"));
                                     QJsonArray variants;
                                     variants.append(variant);
                                     QJsonArray qualityOptions;
                                     for (const int rate : {0, 8, 4, 3, 2}) {
                                         QJsonObject option;
                                         option.insert(QStringLiteral("id"),
                                                       QStringLiteral("rate-%1").arg(rate));
                                         option.insert(QStringLiteral("label"),
                                                       QStringLiteral("quality-%1").arg(rate));
                                         option.insert(QStringLiteral("rate"), rate);
                                         qualityOptions.append(option);
                                     }
                                     QJsonObject response;
                                     response.insert(QStringLiteral("requestId"), static_cast<qint64>(requestId));
                                     response.insert(QStringLiteral("ok"), true);
                                     response.insert(QStringLiteral("roomId"), roomId);
                                     response.insert(QStringLiteral("isLive"), true);
                                     response.insert(QStringLiteral("variants"), variants);
                                     response.insert(QStringLiteral("qualityOptions"), qualityOptions);
                                     emitObject(response);
                                 });
                timer->start(options.delayMs);
            } else if (request->operation == ServiceOperation::Status) {
                const int scriptIndex = options.searchScript.isEmpty()
                    ? 0
                    : qMin(searchCount, options.searchScript.size() - 1);
                const bool online = options.searchScript.isEmpty()
                    ? true
                    : options.searchScript.at(scriptIndex);
                ++searchCount;

                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                response.insert(QStringLiteral("status"), true);
                response.insert(QStringLiteral("isLive"), online);
                emitObject(response);
            } else if (request->operation == ServiceOperation::Search) {
                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                if (options.wrongSearchShape) {
                    response.insert(QStringLiteral("pong"), true);
                    emitObject(response);
                    continue;
                }
                QJsonArray results;
                static const QRegularExpression roomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));
                if (roomIdPattern.match(request->query).hasMatch()) {
                    const int scriptIndex = options.searchScript.isEmpty()
                        ? 0
                        : qMin(searchCount, options.searchScript.size() - 1);
                    const bool online = options.searchScript.isEmpty()
                        ? true
                        : options.searchScript.at(scriptIndex);
                    ++searchCount;

                    QJsonObject result;
                    result.insert(QStringLiteral("roomId"), request->query);
                    result.insert(QStringLiteral("anchorName"), QStringLiteral("Fake Anchor"));
                    result.insert(QStringLiteral("title"), QStringLiteral("Fake Room"));
                    result.insert(QStringLiteral("category"), QStringLiteral("Game"));
                    result.insert(QStringLiteral("online"), online);
                    result.insert(QStringLiteral("viewerLabel"), QStringLiteral("1,234"));
                    result.insert(QStringLiteral("avatarUrl"), QStringLiteral("https://example.invalid/avatar.jpg"));
                    results.append(result);
                }
                response.insert(QStringLiteral("results"), results);
                emitObject(response);
            }
        }
        if (inputClosed.load() && pending.isEmpty()) application.quit();
    });
    poller.start(1);
    const int result = application.exec();
    if (reader.joinable()) reader.join();
    return result;
}
