#include "danmaku/douyu_danmaku_protocol.h"

#include <QtEndian>
#include <QStringConverter>

namespace DouyuDanmakuProtocol {
namespace {

constexpr qsizetype kHeaderBytes = 12;
constexpr qsizetype kLengthAfterFirstField = 8;
constexpr qsizetype kMaxFrameBytes = 1024 * 1024;
constexpr quint16 kServerProtocol = 690;

quint32 readLe32(const char *data)
{
    return qFromLittleEndian<quint32>(data);
}

quint16 readLe16(const char *data)
{
    return qFromLittleEndian<quint16>(data);
}

} // namespace

QString escapeStt(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1String("@"), QLatin1String("@A"));
    escaped.replace(QLatin1String("/"), QLatin1String("@S"));
    return escaped;
}

QString unescapeStt(const QString &value)
{
    QString unescaped = value;
    unescaped.replace(QLatin1String("@S"), QLatin1String("/"));
    unescaped.replace(QLatin1String("@A"), QLatin1String("@"));
    return unescaped;
}

QString serializeStt(const QMap<QString, QString> &fields)
{
    QString serialized;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        serialized += escapeStt(it.key());
        serialized += QLatin1String("@=");
        serialized += escapeStt(it.value());
        serialized += QLatin1Char('/');
    }
    return serialized;
}

QMap<QString, QString> parseStt(const QString &raw)
{
    QMap<QString, QString> parsed;
    const QStringList parts = raw.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const qsizetype separator = part.indexOf(QLatin1String("@="));
        if (separator < 1) continue;
        parsed.insert(unescapeStt(part.left(separator)),
                      unescapeStt(part.mid(separator + 2)));
    }
    return parsed;
}

QByteArray encodeFrame(const QString &payload, quint16 protocol)
{
    const QByteArray payloadBytes = payload.toUtf8();
    const quint32 bodyLength = static_cast<quint32>(kLengthAfterFirstField
                                                    + payloadBytes.size() + 1);
    QByteArray frame(static_cast<qsizetype>(bodyLength) + 4, '\0');
    qToLittleEndian<quint32>(bodyLength, frame.data());
    qToLittleEndian<quint32>(bodyLength, frame.data() + 4);
    qToLittleEndian<quint16>(protocol, frame.data() + 8);
    std::copy(payloadBytes.cbegin(), payloadBytes.cend(), frame.begin() + kHeaderBytes);
    return frame;
}

FrameDecodeResult FrameDecoder::push(QByteArrayView chunk)
{
    pending_.append(chunk.data(), chunk.size());
    FrameDecodeResult result;

    while (pending_.size() >= 4) {
        const quint32 bodyLength = readLe32(pending_.constData());
        if (bodyLength < static_cast<quint32>(kLengthAfterFirstField + 1)) {
            result.error = FrameError::InvalidLength;
            pending_.clear();
            return result;
        }

        const quint64 totalLength = static_cast<quint64>(bodyLength) + 4;
        if (totalLength > static_cast<quint64>(kMaxFrameBytes)) {
            result.error = FrameError::FrameTooLarge;
            pending_.clear();
            return result;
        }
        if (pending_.size() < static_cast<qsizetype>(totalLength)) break;
        if (readLe32(pending_.constData() + 4) != bodyLength) {
            result.error = FrameError::RepeatedLengthMismatch;
            pending_.clear();
            return result;
        }
        if (readLe16(pending_.constData() + 8) != kServerProtocol) {
            result.error = FrameError::UnexpectedProtocol;
            pending_.clear();
            return result;
        }
        if (pending_.at(static_cast<qsizetype>(totalLength) - 1) != '\0') {
            result.error = FrameError::MissingTerminator;
            pending_.clear();
            return result;
        }

        const QByteArray payload = pending_.mid(kHeaderBytes,
                                                static_cast<qsizetype>(totalLength)
                                                    - kHeaderBytes - 1);
        QStringDecoder decoder(QStringConverter::Utf8);
        const QString decoded = decoder(payload);
        if (decoder.hasError()) {
            result.error = FrameError::InvalidUtf8;
            pending_.clear();
            return result;
        }
        result.frames.push_back(decoded);
        pending_.remove(0, static_cast<qsizetype>(totalLength));
    }

    return result;
}

void FrameDecoder::clear()
{
    pending_.clear();
}

} // namespace DouyuDanmakuProtocol
