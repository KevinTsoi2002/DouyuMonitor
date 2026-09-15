#include "app/application_logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

namespace {

QMutex mutex;
QFile *file = nullptr;
QString logPath;
QtMessageHandler previousHandler = nullptr;

QString redact(QString message)
{
    static const QRegularExpression keyValue(
        QStringLiteral(R"(((?:token|cookie|signature|password|passwd|authorization)[[:space:]]*[=:][[:space:]]*)([^[:space:]&;,]+))"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(keyValue, QStringLiteral("\\1[REDACTED]"));

    static const QRegularExpression urlQuery(
        QStringLiteral(R"(([?&](?:token|cookie|signature|password|passwd|authorization)=)([^&#[:space:]]+))"),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(urlQuery, QStringLiteral("\\1[REDACTED]"));
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
    QMutexLocker locker(&mutex);
    if (file != nullptr && file->isOpen()) {
        QTextStream stream(file);
        stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
               << " [" << levelName(type) << "]"
               << " [thread=" << reinterpret_cast<quintptr>(QThread::currentThreadId()) << "] ";
        if (context.category != nullptr && *context.category != '\0') {
            stream << '[' << context.category << "] ";
        }
        stream << redact(message) << Qt::endl;
        file->flush();
    }
    if (previousHandler != nullptr && previousHandler != handler) {
        previousHandler(type, context, message);
    }
}

QString defaultDirectory()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (directory.isEmpty()) directory = QDir::homePath() + QStringLiteral("/.DouyuMonitor");
    return QDir(directory).filePath(QStringLiteral("logs"));
}

} // namespace

void ApplicationLogger::install(const QString &directory)
{
    QMutexLocker locker(&mutex);
    if (file != nullptr) return;

    const QString logDirectory = directory.isEmpty() ? defaultDirectory() : directory;
    QDir().mkpath(logDirectory);
    logPath = QDir(logDirectory).filePath(
        QStringLiteral("DouyuMonitor-%1.log").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    file = new QFile(logPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete file;
        file = nullptr;
        logPath.clear();
        return;
    }
    previousHandler = qInstallMessageHandler(handler);
}

void ApplicationLogger::flush()
{
    QMutexLocker locker(&mutex);
    if (file != nullptr) file->flush();
}

QString ApplicationLogger::currentLogPath()
{
    QMutexLocker locker(&mutex);
    return logPath;
}

void ApplicationLogger::resetForTest()
{
    QMutexLocker locker(&mutex);
    if (file == nullptr) return;
    qInstallMessageHandler(previousHandler);
    file->close();
    delete file;
    file = nullptr;
    logPath.clear();
    previousHandler = nullptr;
}
