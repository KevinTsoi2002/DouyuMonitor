#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest/QtTest>

#include "app/application_logger.h"
#include "app/application_logger_io.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace {

struct BackendStats {
    int writeCalls = 0;
    int flushCalls = 0;
    int closeCalls = 0;
    QVector<QByteArray> writes;
};

struct BackendGate {
    std::atomic_bool writeEntered{false};
    std::atomic_bool releaseWrite{false};
};

class FailingBackend final : public ApplicationLoggerBackend {
public:
    enum class Failure {
        None,
        Open,
        Write,
        PartialWrite,
        Flush,
        Close,
    };

    FailingBackend(Failure failure,
                   std::shared_ptr<BackendStats> stats = {},
                   std::shared_ptr<BackendGate> gate = {})
        : failure_(failure), stats_(std::move(stats)), gate_(std::move(gate))
    {
    }

    bool ensureDirectory(const QString &) override { return true; }
    bool open(const QString &) override { return failure_ != Failure::Open; }
    qint64 write(const QByteArray &data) override
    {
        ++writeCalls_;
        if (stats_) ++stats_->writeCalls;
        if (gate_) {
            gate_->writeEntered.store(true, std::memory_order_release);
            while (!gate_->releaseWrite.load(std::memory_order_acquire)) std::this_thread::yield();
        }
        if (failure_ == Failure::Write) return -1;
        if (failure_ == Failure::PartialWrite) {
            return data.isEmpty() ? 0 : data.size() - 1;
        }
        bytesWritten_ += data.size();
        if (stats_) stats_->writes.append(data);
        return data.size();
    }
    bool flush() override
    {
        ++flushCalls_;
        if (stats_) ++stats_->flushCalls;
        return failure_ != Failure::Flush;
    }
    bool close() override
    {
        ++closeCalls_;
        if (stats_) ++stats_->closeCalls;
        return failure_ != Failure::Close;
    }

    qint64 bytesWritten() const { return bytesWritten_; }
    int writeCalls() const { return writeCalls_; }
    int flushCalls() const { return flushCalls_; }
    int closeCalls() const { return closeCalls_; }

private:
    Failure failure_;
    qint64 bytesWritten_ = 0;
    int writeCalls_ = 0;
    int flushCalls_ = 0;
    int closeCalls_ = 0;
    std::shared_ptr<BackendStats> stats_;
    std::shared_ptr<BackendGate> gate_;
};

}

class ApplicationLoggerTest final : public QObject {
    Q_OBJECT

private slots:
    void writesTimestampedMessagesAndRedactsSensitiveValues();
    void redactsCompleteCredentialsAndRemoteAddresses();
    void exposesInitializationFailure();
    void exposesOpenFailure();
    void exposesInjectedWriteFailure();
    void exposesInjectedPartialWriteFailure();
    void exposesInjectedFlushFailure();
    void exposesInjectedCloseFailure();
    void forwardsAfterFailureWithoutRetrying();
    void preservesFailureAfterSuccessfulShutdown();
    void repeatedInstallKeepsCurrentBackend();
    void restoresPreviousHandlerAfterShutdown();
    void keepsPreviousHandlerWhenInstallFails();
    void forwardsThroughReentrantPreviousHandler();
    void serializesConcurrentWrites();
    void stopsConcurrentWritesAfterFailure();
    void shutdownWaitsForInFlightWrite();
    void canRecoverAfterInitializationFailure();
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
QStringList forwardedMessages;
std::atomic_bool reentrantCallbackTriggered = false;
void captureMessage(QtMsgType, const QMessageLogContext &, const QString &message)
{
    forwarded = message;
    forwardedMessages.append(message);
}

void reentrantMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    captureMessage(type, context, message);
    if (!reentrantCallbackTriggered.exchange(true)) qInfo() << "nested from previous";
}

void discardMessage(QtMsgType, const QMessageLogContext &, const QString &)
{
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

void ApplicationLoggerTest::exposesInitializationFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QFile blocker(directory.filePath(QStringLiteral("not-a-directory")));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    QVERIFY(!ApplicationLogger::install(blocker.fileName()));
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::DirectoryCreateFailed);
    QVERIFY(ApplicationLogger::currentLogPath().isEmpty());
    QVERIFY(ApplicationLogger::shutdown());
    ApplicationLogger::resetForTest();
}

void ApplicationLoggerTest::exposesOpenFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationLogger::setTestBackendFactory([] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Open);
    });
    QVERIFY(!ApplicationLogger::install(directory.path()));
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::OpenFailed);
    QVERIFY(ApplicationLogger::currentLogPath().isEmpty());
    ApplicationLogger::resetForTest();
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::exposesInjectedWriteFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationLogger::setTestBackendFactory([] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Write);
    });
    ApplicationLogger::install(directory.path());
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Ready);
    qInfo() << "injected write failure";
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::WriteFailed);
    ApplicationLogger::resetForTest();
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::exposesInjectedPartialWriteFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationLogger::setTestBackendFactory([] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::PartialWrite);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));
    qInfo() << "injected partial write failure";
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::PartialWrite);
    ApplicationLogger::resetForTest();
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::exposesInjectedFlushFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationLogger::setTestBackendFactory([] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Flush);
    });
    ApplicationLogger::install(directory.path());
    QVERIFY(ApplicationLogger::flush() == false);
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::FlushFailed);
    ApplicationLogger::resetForTest();
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::exposesInjectedCloseFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationLogger::setTestBackendFactory([] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Close);
    });
    ApplicationLogger::install(directory.path());
    QVERIFY(!ApplicationLogger::shutdown());
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::CloseFailed);
    ApplicationLogger::resetForTest();
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::forwardsAfterFailureWithoutRetrying()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    forwardedMessages.clear();
    const auto previous = qInstallMessageHandler(captureMessage);
    const auto stats = std::make_shared<BackendStats>();
    ApplicationLogger::setTestBackendFactory([stats] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Write, stats);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));
    qInfo() << "first injected failure";
    qInfo() << "second injected failure";
    QVERIFY(forwardedMessages.contains(QStringLiteral("first injected failure")));
    QVERIFY(forwardedMessages.contains(QStringLiteral("second injected failure")));
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(stats->writeCalls, 1);
    QVERIFY(!ApplicationLogger::flush());
    QCOMPARE(stats->flushCalls, 0);
    ApplicationLogger::resetForTest();
    QCOMPARE(stats->closeCalls, 1);
    QCOMPARE(stats->flushCalls, 0);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::None);
    qInstallMessageHandler(previous);
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::preservesFailureAfterSuccessfulShutdown()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto stats = std::make_shared<BackendStats>();
    ApplicationLogger::setTestBackendFactory([stats] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Write, stats);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));
    qInfo() << "shutdown preserves failure";
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::WriteFailed);
    QVERIFY(ApplicationLogger::shutdown());
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Closed);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::WriteFailed);
    QCOMPARE(stats->closeCalls, 1);
    QVERIFY(ApplicationLogger::shutdown());
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::WriteFailed);
    QCOMPARE(stats->closeCalls, 1);
    ApplicationLogger::resetForTest();
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::repeatedInstallKeepsCurrentBackend()
{
    QTemporaryDir firstDirectory;
    QTemporaryDir secondDirectory;
    QVERIFY(firstDirectory.isValid());
    QVERIFY(secondDirectory.isValid());
    int factoryCalls = 0;
    ApplicationLogger::setTestBackendFactory([&factoryCalls] {
        ++factoryCalls;
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Close);
    });
    QVERIFY(ApplicationLogger::install(firstDirectory.path()));
    const auto firstPath = ApplicationLogger::currentLogPath();
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(ApplicationLogger::install(secondDirectory.path()));
    QCOMPARE(factoryCalls, 1);
    QCOMPARE(ApplicationLogger::currentLogPath(), firstPath);
    ApplicationLogger::resetForTest();
    ApplicationLogger::clearTestBackendFactory();
}

void ApplicationLoggerTest::restoresPreviousHandlerAfterShutdown()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    forwardedMessages.clear();
    const auto previous = qInstallMessageHandler(captureMessage);
    const auto stats = std::make_shared<BackendStats>();
    ApplicationLogger::setTestBackendFactory([stats] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::None, stats);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));
    qInfo() << "before logger shutdown";
    QVERIFY(ApplicationLogger::shutdown());
    qInfo() << "after logger shutdown";
    QCOMPARE(stats->writes.size(), 1);
    QCOMPARE(forwardedMessages.size(), 2);
    QVERIFY(forwardedMessages.contains(QStringLiteral("before logger shutdown")));
    QVERIFY(forwardedMessages.contains(QStringLiteral("after logger shutdown")));
    qInstallMessageHandler(previous);
    ApplicationLogger::clearTestBackendFactory();
    ApplicationLogger::resetForTest();
}

void ApplicationLoggerTest::keepsPreviousHandlerWhenInstallFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    forwardedMessages.clear();
    const auto previous = qInstallMessageHandler(captureMessage);
    ApplicationLogger::setTestBackendFactory([] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Open);
    });
    QVERIFY(!ApplicationLogger::install(directory.path()));
    qInfo() << "failed install remains on previous handler";
    QCOMPARE(forwardedMessages.size(), 1);
    QCOMPARE(forwardedMessages.first(), QStringLiteral("failed install remains on previous handler"));
    qInstallMessageHandler(previous);
    ApplicationLogger::clearTestBackendFactory();
    ApplicationLogger::resetForTest();
}

void ApplicationLoggerTest::forwardsThroughReentrantPreviousHandler()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    forwardedMessages.clear();
    reentrantCallbackTriggered = false;
    const auto previous = qInstallMessageHandler(reentrantMessageHandler);
    const auto stats = std::make_shared<BackendStats>();
    ApplicationLogger::setTestBackendFactory([stats] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::None, stats);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));
    qInfo() << "outer token=synthetic-secret";
    QVERIFY(ApplicationLogger::shutdown());
    QCOMPARE(stats->writes.size(), 1);
    QCOMPARE(forwardedMessages.size(), 1);
    QVERIFY(forwardedMessages.contains(QStringLiteral("outer token=[REDACTED]")));
    QVERIFY(!forwardedMessages.first().contains(QStringLiteral("synthetic-secret")));
    QVERIFY(reentrantCallbackTriggered.load());
    qInstallMessageHandler(previous);
    ApplicationLogger::clearTestBackendFactory();
    ApplicationLogger::resetForTest();
}

void ApplicationLoggerTest::serializesConcurrentWrites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto previous = qInstallMessageHandler(discardMessage);
    const auto stats = std::make_shared<BackendStats>();
    ApplicationLogger::setTestBackendFactory([stats] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::None, stats);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));

    constexpr int threadCount = 4;
    constexpr int messagesPerThread = 25;
    std::atomic_int ready{0};
    std::atomic_bool start{false};
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.emplace_back([threadIndex, &ready, &start] {
            ++ready;
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int messageIndex = 0; messageIndex < messagesPerThread; ++messageIndex) {
                qInfo().noquote() << QStringLiteral("concurrent-%1-%2")
                                         .arg(threadIndex)
                                         .arg(messageIndex);
            }
        });
    }
    while (ready.load(std::memory_order_acquire) != threadCount) std::this_thread::yield();
    start.store(true, std::memory_order_release);
    for (auto &worker : workers) worker.join();

    QCOMPARE(stats->writes.size(), threadCount * messagesPerThread);
    for (const auto &write : stats->writes) {
        QVERIFY(write.endsWith('\n'));
        QVERIFY(write.contains("concurrent-"));
    }
    QVERIFY(ApplicationLogger::shutdown());
    qInstallMessageHandler(previous);
    ApplicationLogger::clearTestBackendFactory();
    ApplicationLogger::resetForTest();
}

void ApplicationLoggerTest::stopsConcurrentWritesAfterFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto previous = qInstallMessageHandler(discardMessage);
    const auto stats = std::make_shared<BackendStats>();
    ApplicationLogger::setTestBackendFactory([stats] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::Write, stats);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));

    constexpr int threadCount = 4;
    constexpr int messagesPerThread = 25;
    std::atomic_int ready{0};
    std::atomic_bool start{false};
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.emplace_back([threadIndex, &ready, &start] {
            ++ready;
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int messageIndex = 0; messageIndex < messagesPerThread; ++messageIndex) {
                qInfo().noquote() << QStringLiteral("failed-concurrent-%1-%2")
                                         .arg(threadIndex)
                                         .arg(messageIndex);
            }
        });
    }
    while (ready.load(std::memory_order_acquire) != threadCount) std::this_thread::yield();
    start.store(true, std::memory_order_release);
    for (auto &worker : workers) worker.join();

    QCOMPARE(stats->writeCalls, 1);
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::WriteFailed);
    QVERIFY(!ApplicationLogger::flush());
    QVERIFY(ApplicationLogger::shutdown());
    qInstallMessageHandler(previous);
    ApplicationLogger::clearTestBackendFactory();
    ApplicationLogger::resetForTest();
}

void ApplicationLoggerTest::shutdownWaitsForInFlightWrite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto previous = qInstallMessageHandler(discardMessage);
    const auto stats = std::make_shared<BackendStats>();
    const auto gate = std::make_shared<BackendGate>();
    ApplicationLogger::setTestBackendFactory([stats, gate] {
        return std::make_unique<FailingBackend>(FailingBackend::Failure::None, stats, gate);
    });
    QVERIFY(ApplicationLogger::install(directory.path()));

    std::thread writer([] {
        qInfo() << "in-flight message";
    });
    while (!gate->writeEntered.load(std::memory_order_acquire)) std::this_thread::yield();

    std::atomic_bool shutdownFinished{false};
    std::atomic_bool shutdownResult{false};
    std::thread closer([&shutdownFinished, &shutdownResult] {
        shutdownResult.store(ApplicationLogger::shutdown(), std::memory_order_release);
        shutdownFinished.store(true, std::memory_order_release);
    });
    std::this_thread::yield();
    QVERIFY(!shutdownFinished.load(std::memory_order_acquire));
    gate->releaseWrite.store(true, std::memory_order_release);
    writer.join();
    closer.join();

    QVERIFY(shutdownFinished.load(std::memory_order_acquire));
    QVERIFY(shutdownResult.load(std::memory_order_acquire));
    QCOMPARE(stats->writeCalls, 1);
    QCOMPARE(stats->closeCalls, 1);
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Closed);
    qInstallMessageHandler(previous);
    ApplicationLogger::clearTestBackendFactory();
    ApplicationLogger::resetForTest();
}

void ApplicationLoggerTest::canRecoverAfterInitializationFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationLogger::resetForTest();
    QFile blocker(directory.filePath(QStringLiteral("blocked")));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    QVERIFY(!ApplicationLogger::install(blocker.fileName()));
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Unavailable);

    QVERIFY(ApplicationLogger::install(directory.path()));
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Ready);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::None);
    qInfo() << "recovered logger";
    QVERIFY(ApplicationLogger::flush());
    QVERIFY(ApplicationLogger::shutdown());
    QCOMPARE(ApplicationLogger::state(), ApplicationLogger::State::Closed);
    QCOMPARE(ApplicationLogger::lastError(), ApplicationLogger::Error::None);
    ApplicationLogger::resetForTest();
}

QTEST_GUILESS_MAIN(ApplicationLoggerTest)

#include "application_logger_test.moc"
