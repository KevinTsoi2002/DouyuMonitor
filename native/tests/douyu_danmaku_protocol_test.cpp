#include <QtEndian>
#include <QtTest/QtTest>

#include "danmaku/douyu_danmaku_protocol.h"

class DouyuDanmakuProtocolTest final : public QObject {
    Q_OBJECT

private slots:
    void encodesLoginFrameWithLittleEndianLengths();
    void decodesFragmentedServerFrames();
    void decodesMultipleFramesFromOneChunk();
    void rejectsRepeatedLengthMismatch();
    void rejectsUnexpectedServerProtocol();
    void rejectsMissingTerminator();
    void rejectsInvalidUtf8();
    void rejectsOversizedFrames();
    void acceptsFrameAtMaximumSize();
    void boundsFramesDecodedPerPush();
    void rejectsOversizedPendingData();
    void roundTripsEscapedSttFields();
};

void DouyuDanmakuProtocolTest::encodesLoginFrameWithLittleEndianLengths()
{
    const QByteArray frame = DouyuDanmakuProtocol::encodeFrame(
        DouyuDanmakuProtocol::serializeStt({{QStringLiteral("type"), QStringLiteral("loginreq")},
                                             {QStringLiteral("roomid"), QStringLiteral("63136")}}));
    QCOMPARE(qFromLittleEndian<quint32>(frame.constData()), quint32(frame.size() - 4));
    QCOMPARE(qFromLittleEndian<quint16>(frame.constData() + 8), quint16(689));
    QCOMPARE(frame.back(), '\0');
}

void DouyuDanmakuProtocolTest::decodesFragmentedServerFrames()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    const QByteArray frame = DouyuDanmakuProtocol::encodeFrame(
        QStringLiteral("type@=chatmsg/rid@=63136/txt@=hello/"), 690);
    QCOMPARE(decoder.push(QByteArrayView(frame).first(7)).frames, QStringList{});
    const auto result = decoder.push(QByteArrayView(frame).sliced(7));
    QCOMPARE(result.error, DouyuDanmakuProtocol::FrameError::None);
    QCOMPARE(result.frames, QStringList({QStringLiteral("type@=chatmsg/rid@=63136/txt@=hello/")}));
}

void DouyuDanmakuProtocolTest::decodesMultipleFramesFromOneChunk()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    const QByteArray first = DouyuDanmakuProtocol::encodeFrame(QStringLiteral("type@=loginres/"), 690);
    const QByteArray second = DouyuDanmakuProtocol::encodeFrame(QStringLiteral("type@=setmsggroup/"), 690);
    const auto result = decoder.push(first + second);
    QCOMPARE(result.error, DouyuDanmakuProtocol::FrameError::None);
    QCOMPARE(result.frames, QStringList({QStringLiteral("type@=loginres/"), QStringLiteral("type@=setmsggroup/")}));
}

void DouyuDanmakuProtocolTest::rejectsRepeatedLengthMismatch()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    QByteArray frame = DouyuDanmakuProtocol::encodeFrame(QStringLiteral("type@=loginres/"), 690);
    qToLittleEndian<quint32>(qFromLittleEndian<quint32>(frame.constData()) + 1U, frame.data() + 4);
    QCOMPARE(decoder.push(frame).error, DouyuDanmakuProtocol::FrameError::RepeatedLengthMismatch);
}

void DouyuDanmakuProtocolTest::rejectsUnexpectedServerProtocol()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    const QByteArray frame = DouyuDanmakuProtocol::encodeFrame(QStringLiteral("type@=loginres/"), 689);
    QCOMPARE(decoder.push(frame).error, DouyuDanmakuProtocol::FrameError::UnexpectedProtocol);
}

void DouyuDanmakuProtocolTest::rejectsMissingTerminator()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    QByteArray frame = DouyuDanmakuProtocol::encodeFrame(QStringLiteral("type@=loginres/"), 690);
    frame.back() = 'x';
    QCOMPARE(decoder.push(frame).error, DouyuDanmakuProtocol::FrameError::MissingTerminator);
}

void DouyuDanmakuProtocolTest::rejectsInvalidUtf8()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    QByteArray frame = DouyuDanmakuProtocol::encodeFrame(QStringLiteral("type@=x/"), 690);
    frame[12 + QByteArrayLiteral("type@=").size()] = char(0xFF);
    QCOMPARE(decoder.push(frame).error, DouyuDanmakuProtocol::FrameError::InvalidUtf8);
}

void DouyuDanmakuProtocolTest::rejectsOversizedFrames()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    QByteArray frame(12, '\0');
    qToLittleEndian<quint32>(1024U * 1024U + 1U, frame.data());
    QCOMPARE(decoder.push(frame).error, DouyuDanmakuProtocol::FrameError::FrameTooLarge);
}

void DouyuDanmakuProtocolTest::acceptsFrameAtMaximumSize()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    const QString payload((1024 * 1024) - 13, QChar('x'));
    const QByteArray frame = DouyuDanmakuProtocol::encodeFrame(payload, 690);
    QCOMPARE(frame.size(), 1024 * 1024);
    const auto result = decoder.push(frame);
    QCOMPARE(result.error, DouyuDanmakuProtocol::FrameError::None);
    QCOMPARE(result.frames.size(), 1);
    QCOMPARE(result.frames.constFirst(), payload);
}

void DouyuDanmakuProtocolTest::boundsFramesDecodedPerPush()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    QByteArray chunk;
    for (int i = 0; i < 70; ++i) {
        chunk += DouyuDanmakuProtocol::encodeFrame(
            QStringLiteral("type@=chatmsg/id@=") + QString::number(i) + QLatin1Char('/'),
            690);
    }

    const auto first = decoder.push(chunk);
    QVERIFY(first.frames.size() < 70);
    QVERIFY(!first.frames.isEmpty());
    QCOMPARE(first.frames.constFirst(), QStringLiteral("type@=chatmsg/id@=0/"));

    QStringList allFrames = first.frames;
    while (true) {
        const auto next = decoder.push({});
        QCOMPARE(next.error, DouyuDanmakuProtocol::FrameError::None);
        allFrames += next.frames;
        if (next.frames.isEmpty()) break;
    }
    QCOMPARE(allFrames.size(), 70);
    for (int i = 0; i < allFrames.size(); ++i) {
        QCOMPARE(allFrames.at(i),
                 QStringLiteral("type@=chatmsg/id@=") + QString::number(i) + QLatin1Char('/'));
    }
}

void DouyuDanmakuProtocolTest::rejectsOversizedPendingData()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    const QByteArray oversized(4 * 1024 * 1024 + 1, '\0');
    const auto result = decoder.push(oversized);
    QCOMPARE(result.error, DouyuDanmakuProtocol::FrameError::PendingDataTooLarge);
}

void DouyuDanmakuProtocolTest::roundTripsEscapedSttFields()
{
    const auto parsed = DouyuDanmakuProtocol::parseStt(
        DouyuDanmakuProtocol::serializeStt({{QStringLiteral("txt"), QStringLiteral("a@b/c")}}));
    QCOMPARE(parsed.value(QStringLiteral("txt")), QStringLiteral("a@b/c"));
}

QTEST_GUILESS_MAIN(DouyuDanmakuProtocolTest)
#include "douyu_danmaku_protocol_test.moc"
