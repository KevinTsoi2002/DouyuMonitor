#include <QtTest/QtTest>

#include "service/stream_service_protocol.h"

class StreamServiceProtocolTest final : public QObject {
    Q_OBJECT

private slots:
    void encodesPingRequest();
    void encodesResolveRequest();
    void decodesValidSuccessResponse();
    void decodesOfflineResponseWithoutUrl();
    void decodesFixedErrorWithoutMessage();
    void rejectsMalformedAndInvalidResponses();
    void rejectsDuplicateVariantsAndCredentialUrls();
};

void StreamServiceProtocolTest::encodesPingRequest()
{
    ServiceRequest request;
    request.requestId = 1;
    request.operation = ServiceOperation::Ping;

    const auto decoded = decodeRequest(encodeRequest(request));
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->requestId, quint64(1));
    QCOMPARE(decoded->operation, ServiceOperation::Ping);
}

void StreamServiceProtocolTest::encodesResolveRequest()
{
    ServiceRequest request;
    request.requestId = 2;
    request.operation = ServiceOperation::Resolve;
    request.roomId = QStringLiteral("63136");
    request.quality = StreamQuality::Auto;

    const auto decoded = decodeRequest(encodeRequest(request));
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->requestId, quint64(2));
    QCOMPARE(decoded->operation, ServiceOperation::Resolve);
    QCOMPARE(decoded->roomId, QStringLiteral("63136"));
    QCOMPARE(decoded->quality, StreamQuality::Auto);
}

void StreamServiceProtocolTest::decodesValidSuccessResponse()
{
    const auto response = decodeResponse(
        QByteArray(R"({"requestId":2,"ok":true,"roomId":"63136","isLive":true,"variants":[{"id":"flv-auto","label":"StreamGet FLV","quality":"auto","container":"flv","playbackUrl":"https://live.douyucdn2.cn/live/63136.flv?wsAuth=redacted"}]})"));

    QVERIFY(response.has_value());
    QCOMPARE(response->requestId, quint64(2));
    QVERIFY(response->ok);
    QCOMPARE(response->roomId, QStringLiteral("63136"));
    QCOMPARE(response->variants.size(), 1);
    QCOMPARE(response->variants.front().quality, StreamQuality::Auto);
    QCOMPARE(response->variants.front().playbackUrl.host(), QStringLiteral("live.douyucdn2.cn"));
}

void StreamServiceProtocolTest::decodesOfflineResponseWithoutUrl()
{
    const auto response = decodeResponse(
        QByteArray(R"({"requestId":4,"ok":true,"roomId":"63136","isLive":false,"variants":[]})"));

    QVERIFY(response.has_value());
    QVERIFY(response->ok);
    QVERIFY(!response->isLive);
    QVERIFY(response->variants.isEmpty());
}

void StreamServiceProtocolTest::decodesFixedErrorWithoutMessage()
{
    const auto response = decodeResponse(
        QByteArray(R"({"requestId":5,"ok":false,"error":{"code":"TIMEOUT","retryable":true}})"));

    QVERIFY(response.has_value());
    QVERIFY(!response->ok);
    QCOMPARE(response->errorCode, QStringLiteral("TIMEOUT"));
    QVERIFY(response->retryable);
    QVERIFY(response->errorMessage.isEmpty());
}

void StreamServiceProtocolTest::rejectsMalformedAndInvalidResponses()
{
    const QList<QByteArray> invalid = {
        QByteArray("not-json"),
        QByteArray(R"({"ok":true,"roomId":"63136","isLive":false,"variants":[]})"),
        QByteArray(R"({"requestId":1,"ok":true,"roomId":"abc","isLive":false,"variants":[]})"),
        QByteArray(R"({"requestId":1,"ok":true,"roomId":"63136","isLive":true,"variants":[]})"),
        QByteArray(R"({"requestId":1,"ok":true,"roomId":"63136","isLive":true,"variants":[{"id":"x","label":"x","quality":"lossless","container":"flv","playbackUrl":"https://live.douyucdn.cn/x.flv"}]})"),
        QByteArray(R"({"requestId":1,"ok":true,"roomId":"63136","isLive":true,"variants":[{"id":"x","label":"x","quality":"auto","container":"flv","playbackUrl":"ftp://live.douyucdn.cn/x.flv"}]})"),
        QByteArray(R"({"requestId":1,"ok":true,"roomId":"63136","isLive":true,"variants":[{"id":"x","label":"x","quality":"auto","container":"flv","playbackUrl":"https://example.invalid/x.flv"}]})"),
    };

    for (const QByteArray &line : invalid) {
        QVERIFY2(!decodeResponse(line).has_value(), line.constData());
    }
}

void StreamServiceProtocolTest::rejectsDuplicateVariantsAndCredentialUrls()
{
    const QByteArray duplicate = R"({"requestId":1,"ok":true,"roomId":"63136","isLive":true,"variants":[{"id":"x","label":"x","quality":"auto","container":"flv","playbackUrl":"https://live.douyucdn.cn/x.flv"},{"id":"x","label":"x2","quality":"auto","container":"flv","playbackUrl":"https://live.douyucdn.cn/y.flv"}]})";
    const QByteArray credentials = R"({"requestId":1,"ok":true,"roomId":"63136","isLive":true,"variants":[{"id":"x","label":"x","quality":"auto","container":"flv","playbackUrl":"https://user:secret@live.douyucdn.cn/x.flv"}]})";

    QVERIFY(!decodeResponse(duplicate).has_value());
    QVERIFY(!decodeResponse(credentials).has_value());
}

QTEST_GUILESS_MAIN(StreamServiceProtocolTest)

#include "stream_service_protocol_test.moc"
