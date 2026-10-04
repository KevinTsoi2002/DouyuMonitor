#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "app/application_logger.h"

class ApplicationLoggerTest final : public QObject {
    Q_OBJECT

private slots:
    void writesTimestampedMessagesAndRedactsSensitiveValues();
    void redactsCompleteCredentialsAndRemoteAddresses();
};

void ApplicationLoggerTest::writesTimestampedMessagesAndRedactsSensitiveValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationLogger::install(directory.path());
    qInfo().noquote() << "logger test token=secret-value cookie=abc123"
                      << "https://example.invalid/path?signature=hidden&room=63136";
    ApplicationLogger::flush();

    const QString logPath = ApplicationLogger::currentLogPath();
    QVERIFY(QFile::exists(logPath));
    QFile logFile(logPath);
    QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(logFile.readAll());
    QVERIFY(contents.contains(QStringLiteral("[INFO]")));
    QVERIFY(contents.contains(QStringLiteral("token=[REDACTED]")));
    QVERIFY(contents.contains(QStringLiteral("cookie=[REDACTED]")));
    QVERIFY(contents.contains(QStringLiteral("[REMOTE_URL_REDACTED]")));
    QVERIFY(!contents.contains(QStringLiteral("secret-value")));
    QVERIFY(!contents.contains(QStringLiteral("abc123")));
    QVERIFY(!contents.contains(QStringLiteral("hidden")));

    ApplicationLogger::resetForTest();
}

namespace {
QString forwarded;
void captureMessage(QtMsgType, const QMessageLogContext &, const QString &message)
{
    forwarded = message;
}
}

void ApplicationLoggerTest::redactsCompleteCredentialsAndRemoteAddresses()
{
    QTemporaryDir directory;
    const auto previous = qInstallMessageHandler(captureMessage);
    ApplicationLogger::install(directory.path());
    qWarning().noquote() << "Authorization: Bearer synthetic-bearer\n"
                        << "Cookie: first=synthetic-cookie; second=synthetic-second\n"
                        << R"({"access_token":"synthetic-json"})"
                        << "https://example.invalid/live?wsSecret=synthetic-signature";
    ApplicationLogger::flush();
    QFile file(ApplicationLogger::currentLogPath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto contents = QString::fromUtf8(file.readAll());
    ApplicationLogger::resetForTest();
    qInstallMessageHandler(previous);
    for (const auto &secret : {"synthetic-bearer", "synthetic-cookie", "synthetic-second",
                               "synthetic-json", "synthetic-signature", "example.invalid"}) {
        QVERIFY2(!contents.contains(QString::fromLatin1(secret)), secret);
        QVERIFY2(!forwarded.contains(QString::fromLatin1(secret)), secret);
    }
}

QTEST_GUILESS_MAIN(ApplicationLoggerTest)

#include "application_logger_test.moc"
