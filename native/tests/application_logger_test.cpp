#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "app/application_logger.h"

class ApplicationLoggerTest final : public QObject {
    Q_OBJECT

private slots:
    void writesTimestampedMessagesAndRedactsSensitiveValues();
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
    QVERIFY(contents.contains(QStringLiteral("signature=[REDACTED]")));
    QVERIFY(!contents.contains(QStringLiteral("secret-value")));
    QVERIFY(!contents.contains(QStringLiteral("abc123")));
    QVERIFY(!contents.contains(QStringLiteral("hidden")));

    ApplicationLogger::resetForTest();
}

QTEST_GUILESS_MAIN(ApplicationLoggerTest)

#include "application_logger_test.moc"
