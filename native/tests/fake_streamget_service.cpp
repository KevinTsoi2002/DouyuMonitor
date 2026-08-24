#include "service/stream_service_protocol.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <atomic>
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
    bool ignoreCancel = false;
    int crashAfter = 0;
    QString crashOnceFile;
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
        } else if (argument == QStringLiteral("--ignore-cancel")) {
            options.ignoreCancel = true;
        } else if (argument == QStringLiteral("--crash-after") && index + 1 < arguments.size()) {
            options.crashAfter = qMax(0, arguments.at(++index).toInt());
        } else if (argument == QStringLiteral("--crash-once-file") && index + 1 < arguments.size()) {
            options.crashOnceFile = arguments.at(++index);
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
    int requestCount = 0;
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
                    std::_Exit(17);
                }
            }
            if (options.malformed) {
                std::cout << "not-json\n" << std::flush;
                continue;
            }

            if (request->operation == ServiceOperation::Ping) {
                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                response.insert(QStringLiteral("pong"), true);
                emitObject(response);
            } else if (request->operation == ServiceOperation::Shutdown) {
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
                auto *timer = new QTimer(&application);
                timer->setSingleShot(true);
                pending.insert(request->requestId, timer);
                QObject::connect(timer, &QTimer::timeout, &application,
                                 [&, timer, requestId = request->requestId, roomId = request->roomId] {
                                     pending.remove(requestId);
                                     timer->deleteLater();
                                     QJsonObject variant;
                                     variant.insert(QStringLiteral("id"), QStringLiteral("flv-auto"));
                                     variant.insert(QStringLiteral("label"), QStringLiteral("fake"));
                                     variant.insert(QStringLiteral("quality"), QStringLiteral("auto"));
                                     variant.insert(QStringLiteral("container"), QStringLiteral("flv"));
                                     variant.insert(QStringLiteral("playbackUrl"),
                                                   QStringLiteral("https://live.douyucdn.cn/fake.flv"));
                                     QJsonArray variants;
                                     variants.append(variant);
                                     QJsonObject response;
                                     response.insert(QStringLiteral("requestId"), static_cast<qint64>(requestId));
                                     response.insert(QStringLiteral("ok"), true);
                                     response.insert(QStringLiteral("roomId"), roomId);
                                     response.insert(QStringLiteral("isLive"), true);
                                     response.insert(QStringLiteral("variants"), variants);
                                     emitObject(response);
                                 });
                timer->start(options.delayMs);
            } else if (request->operation == ServiceOperation::Search) {
                QJsonObject response;
                response.insert(QStringLiteral("requestId"), static_cast<qint64>(request->requestId));
                response.insert(QStringLiteral("ok"), true);
                response.insert(QStringLiteral("results"), QJsonArray());
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
