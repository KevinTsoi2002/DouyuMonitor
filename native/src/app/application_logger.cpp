#include "app/application_logger.h"

#include "app/application_logger_io.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

#include <utility>

namespace {

class QFileBackend final : public ApplicationLoggerBackend {
public:
    bool ensureDirectory(const QString &directory) override { return QDir().mkpath(directory); }
    bool open(const QString &path) override
    {
        file_.setFileName(path);
        return file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    qint64 write(const QByteArray &data) override { return file_.write(data); }
    bool flush() override { return file_.flush(); }
    bool close() override
    {
        if (!file_.isOpen()) return true;
        const bool flushed = file_.flush();
        file_.close();
        return flushed && file_.error() == QFileDevice::NoError;
    }

private:
    QFile file_;
};

QMutex mutex;
std::unique_ptr<ApplicationLoggerBackend> backend;
QString logPath;
QtMessageHandler previousHandler = nullptr;
bool handlerInstalled = false;
ApplicationLogger::State loggerState = ApplicationLogger::State::Uninstalled;
ApplicationLogger::Error loggerError = ApplicationLogger::Error::None;

#ifdef DOUYU_APPLICATION_LOGGER_TESTING
std::function<std::unique_ptr<ApplicationLoggerBackend>()> testBackendFactory;
#endif

QString redact(QString message)
{
    static const QRegularExpression headers(
        QStringLiteral(R"((\b(?:authorization|proxy-authorization|cookie|set-cookie)\s*:\s*)[^\r\n]+)"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(headers, QStringLiteral("\\1[REDACTED]"));
    static const QRegularExpression jsonValues(
        QStringLiteral(R"re(("(?:access_token|refresh_token|token|cookie|signature|password|passwd|authorization)"\s*:\s*)"(?:\\.|[^"\\])*")re"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(jsonValues, QStringLiteral("\\1\"[REDACTED]\""));
    static const QRegularExpression keyValue(
        QStringLiteral(R"(((?:access_token|refresh_token|token|cookie|signature|password|passwd|authorization)[[:space:]]*[=:][[:space:]]*)([^[:space:]&;,]+))"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(keyValue, QStringLiteral("\\1[REDACTED]"));

    static const QRegularExpression urlQuery(
        QStringLiteral(R"(([?&](?:token|cookie|signature|password|passwd|authorization)=)([^&#[:space:]]+))"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(urlQuery, QStringLiteral("\\1[REDACTED]"));
    static const QRegularExpression remoteUrl(QStringLiteral(R"(https?://[^\s<>"']+)"),
                                              QRegularExpression::CaseInsensitiveOption);
    message.replace(remoteUrl, QStringLiteral("[REMOTE_URL_REDACTED]"));
    return message;
}

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return QStringLiteral("DEBUG");
    case QtInfoMsg: return QStringLiteral("INFO");
    case QtWarningMsg: return QStringLiteral("WARN");
    case QtCriticalMsg: return QStringLiteral("ERROR");
    case QtFatalMsg: return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

void handler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const auto safeMessage = redact(message);
    QtMessageHandler forward = nullptr;
    {
        QMutexLocker locker(&mutex);
        if (loggerState == ApplicationLogger::State::Ready && backend != nullptr) {
            QString line = QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                + QStringLiteral(" [") + levelName(type)
                + QStringLiteral("] [thread=")
                + QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()))
                + QStringLiteral("] ");
            if (context.category != nullptr && *context.category != '\0') {
                line += QStringLiteral("[") + QString::fromUtf8(context.category) + QStringLiteral("] ");
            }
            line += safeMessage;
            line += QLatin1Char('\n');
            const auto data = line.toUtf8();
            const qint64 written = backend->write(data);
            if (written < 0) {
                loggerState = ApplicationLogger::State::Unavailable;
                loggerError = ApplicationLogger::Error::WriteFailed;
            } else if (written != data.size()) {
                loggerState = ApplicationLogger::State::Unavailable;
                loggerError = ApplicationLogger::Error::PartialWrite;
            } else if (!backend->flush()) {
                loggerState = ApplicationLogger::State::Unavailable;
                loggerError = ApplicationLogger::Error::FlushFailed;
            }
        }
        if (previousHandler != nullptr && previousHandler != handler) forward = previousHandler;
    }
    if (forward != nullptr) forward(type, context, safeMessage);
}

QString defaultDirectory()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (directory.isEmpty()) directory = QDir::homePath() + QStringLiteral("/.DouyuMonitor");
    return QDir(directory).filePath(QStringLiteral("logs"));
}

std::unique_ptr<ApplicationLoggerBackend> makeBackend()
{
#ifdef DOUYU_APPLICATION_LOGGER_TESTING
    if (testBackendFactory) return testBackendFactory();
#endif
    return std::make_unique<QFileBackend>();
}

} // namespace

bool ApplicationLogger::install(const QString &directory)
{
    QMutexLocker locker(&mutex);
    if (backend != nullptr) return loggerState == State::Ready;

    const QString logDirectory = directory.isEmpty() ? defaultDirectory() : directory;
    auto candidate = makeBackend();
    if (!candidate || !candidate->ensureDirectory(logDirectory)) {
        loggerState = State::Unavailable;
        loggerError = Error::DirectoryCreateFailed;
        logPath.clear();
        return false;
    }
    logPath = QDir(logDirectory).filePath(
        QStringLiteral("DouyuMonitor-%1.log").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    if (!candidate->open(logPath)) {
        loggerState = State::Unavailable;
        loggerError = Error::OpenFailed;
        logPath.clear();
        return false;
    }
    backend = std::move(candidate);
    previousHandler = qInstallMessageHandler(handler);
    handlerInstalled = true;
    loggerState = State::Ready;
    loggerError = Error::None;
    return true;
}

bool ApplicationLogger::flush()
{
    QMutexLocker locker(&mutex);
    if (backend == nullptr || loggerState != State::Ready) return false;
    if (!backend->flush()) {
        loggerState = State::Unavailable;
        loggerError = Error::FlushFailed;
        return false;
    }
    return true;
}

bool ApplicationLogger::shutdown()
{
    QMutexLocker locker(&mutex);
    if (backend == nullptr) {
        if (loggerState == State::Uninstalled) loggerState = State::Closed;
        return true;
    }
    const auto restore = previousHandler;
    if (handlerInstalled) qInstallMessageHandler(restore);
    handlerInstalled = false;
    previousHandler = nullptr;
    const auto priorError = loggerError;
    const bool closed = backend->close();
    if (!closed) {
        loggerState = State::Unavailable;
        loggerError = Error::CloseFailed;
    } else {
        loggerState = State::Closed;
        // Keep the most recent I/O failure observable after the handle is closed.
        loggerError = priorError;
    }
    backend.reset();
    logPath.clear();
    return closed;
}

QString ApplicationLogger::currentLogPath()
{
    QMutexLocker locker(&mutex);
    return logPath;
}

void ApplicationLogger::resetForTest()
{
    shutdown();
    QMutexLocker locker(&mutex);
    loggerState = State::Uninstalled;
    loggerError = Error::None;
#ifdef DOUYU_APPLICATION_LOGGER_TESTING
    testBackendFactory = {};
#endif
}

ApplicationLogger::State ApplicationLogger::state()
{
    QMutexLocker locker(&mutex);
    return loggerState;
}

ApplicationLogger::Error ApplicationLogger::lastError()
{
    QMutexLocker locker(&mutex);
    return loggerError;
}

#ifdef DOUYU_APPLICATION_LOGGER_TESTING
void ApplicationLogger::setTestBackendFactory(
    std::function<std::unique_ptr<ApplicationLoggerBackend>()> factory)
{
    QMutexLocker locker(&mutex);
    testBackendFactory = std::move(factory);
}

void ApplicationLogger::clearTestBackendFactory()
{
    QMutexLocker locker(&mutex);
    testBackendFactory = {};
}
#endif
