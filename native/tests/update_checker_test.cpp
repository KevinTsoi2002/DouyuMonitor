#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest/QtTest>

#include "app/update_checker.h"

namespace {

class HttpFixture final : public QObject {
    Q_OBJECT

public:
    explicit HttpFixture(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&server_, &QTcpServer::newConnection, this, [this] {
            auto *socket = server_.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                socket->readAll();
                if (respondedSockets_.value(socket, false)) return;
                respondedSockets_.insert(socket, true);
                if (delayMs_ > 0) {
                    QTimer::singleShot(delayMs_, socket, [this, socket] { writeResponse(socket); });
                } else {
                    writeResponse(socket);
                }
            });
        });
    }

    bool listen()
    {
        return server_.listen(QHostAddress::LocalHost);
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/releases/latest").arg(server_.serverPort()));
    }

    int statusCode_ = 200;
    QByteArray body_;
    int delayMs_ = 0;

private:
    void writeResponse(QTcpSocket *socket)
    {
        const QByteArray reason = statusCode_ == 200 ? QByteArrayLiteral("OK")
                                                      : QByteArrayLiteral("Error");
        const QByteArray response = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(statusCode_)
            + ' ' + reason + QByteArrayLiteral("\r\nContent-Type: application/json\r\nContent-Length: ")
            + QByteArray::number(body_.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body_;
        socket->write(response);
        socket->disconnectFromHost();
    }

    QTcpServer server_;
    QHash<QTcpSocket *, bool> respondedSockets_;
};

QByteArray releaseBody(const QByteArray &tag = QByteArrayLiteral("v0.2.4"))
{
    QJsonObject object;
    object.insert(QStringLiteral("tag_name"), QString::fromUtf8(tag));
    object.insert(QStringLiteral("html_url"), QStringLiteral("https://github.com/KevinTsoi2002/DouyuMonitor/releases/tag/%1")
                                                      .arg(QString::fromUtf8(tag)));
    object.insert(QStringLiteral("draft"), false);
    object.insert(QStringLiteral("prerelease"), false);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

class UpdateCheckerTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesSupportedVersionTags();
    void comparesThreePartVersions();
    void rejectsInvalidVersionTags();
    void reportsUpdateAvailableFromLatestRelease();
    void reportsHttpErrors();
    void reportsInvalidJson();
    void reportsTimeout();
    void ignoresFinishedSignalFromReplacedRequest();
};

void UpdateCheckerTest::parsesSupportedVersionTags()
{
    QCOMPARE(UpdateChecker::normalizeVersionTag(QStringLiteral("V0.2.3")), QStringLiteral("0.2.3"));
    QCOMPARE(UpdateChecker::normalizeVersionTag(QStringLiteral("v1.20.300")), QStringLiteral("1.20.300"));
    QCOMPARE(UpdateChecker::normalizeVersionTag(QStringLiteral("0.0.1")), QStringLiteral("0.0.1"));
}

void UpdateCheckerTest::comparesThreePartVersions()
{
    QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.2.4"), QStringLiteral("0.2.3")) > 0);
    QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v0.2.3"), QStringLiteral("V0.2.3")), 0);
    QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.10.0"), QStringLiteral("0.9.99")) > 0);
}

void UpdateCheckerTest::rejectsInvalidVersionTags()
{
    QVERIFY(UpdateChecker::normalizeVersionTag(QStringLiteral("release" )).isEmpty());
    QVERIFY(UpdateChecker::normalizeVersionTag(QStringLiteral("1.2" )).isEmpty());
    QVERIFY(UpdateChecker::normalizeVersionTag(QStringLiteral("1.2.3-beta" )).isEmpty());
}

void UpdateCheckerTest::reportsUpdateAvailableFromLatestRelease()
{
    HttpFixture fixture;
    QVERIFY(fixture.listen());
    fixture.body_ = releaseBody();
    UpdateChecker checker(QStringLiteral("0.2.3"), fixture.url(), 500);
    QSignalSpy stateSpy(&checker, &UpdateChecker::stateChanged);

    checker.check();

    QTRY_COMPARE_WITH_TIMEOUT(checker.state(), UpdateChecker::State::UpdateAvailable, 2000);
    QCOMPARE(checker.latestVersion(), QStringLiteral("0.2.4"));
    QCOMPARE(checker.releaseUrl().toString(), QStringLiteral("https://github.com/KevinTsoi2002/DouyuMonitor/releases/tag/v0.2.4"));
    QVERIFY(stateSpy.count() >= 2);
}

void UpdateCheckerTest::reportsHttpErrors()
{
    HttpFixture fixture;
    QVERIFY(fixture.listen());
    fixture.statusCode_ = 500;
    fixture.body_ = QByteArrayLiteral("{}");
    UpdateChecker checker(QStringLiteral("0.2.3"), fixture.url(), 500);

    checker.check();

    QTRY_COMPARE_WITH_TIMEOUT(checker.state(), UpdateChecker::State::Error, 2000);
    QVERIFY(!checker.errorMessage().isEmpty());
}

void UpdateCheckerTest::reportsInvalidJson()
{
    HttpFixture fixture;
    QVERIFY(fixture.listen());
    fixture.body_ = QByteArrayLiteral("not-json");
    UpdateChecker checker(QStringLiteral("0.2.3"), fixture.url(), 500);

    checker.check();

    QTRY_COMPARE_WITH_TIMEOUT(checker.state(), UpdateChecker::State::Error, 2000);
}

void UpdateCheckerTest::reportsTimeout()
{
    HttpFixture fixture;
    QVERIFY(fixture.listen());
    fixture.body_ = releaseBody();
    fixture.delayMs_ = 1000;
    UpdateChecker checker(QStringLiteral("0.2.3"), fixture.url(), 30);

    checker.check();

    QTRY_COMPARE_WITH_TIMEOUT(checker.state(), UpdateChecker::State::Error, 2000);
    QVERIFY(checker.errorMessage().contains(QStringLiteral("超时")));
}

void UpdateCheckerTest::ignoresFinishedSignalFromReplacedRequest()
{
    HttpFixture fixture;
    QVERIFY(fixture.listen());
    fixture.body_ = releaseBody();
    fixture.delayMs_ = 100;
    UpdateChecker checker(QStringLiteral("0.2.3"), fixture.url(), 500);

    checker.check();
    checker.check();

    QTRY_COMPARE_WITH_TIMEOUT(checker.state(), UpdateChecker::State::UpdateAvailable, 2000);
    QCOMPARE(checker.latestVersion(), QStringLiteral("0.2.4"));
}

QTEST_GUILESS_MAIN(UpdateCheckerTest)

#include "update_checker_test.moc"
