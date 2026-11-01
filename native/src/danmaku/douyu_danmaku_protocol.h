#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QMap>
#include <QString>
#include <QStringList>

namespace DouyuDanmakuProtocol {

enum class FrameError {
    None,
    InvalidLength,
    RepeatedLengthMismatch,
    UnexpectedProtocol,
    MissingTerminator,
    InvalidUtf8,
    FrameTooLarge,
    PendingDataTooLarge,
};

inline constexpr qsizetype kMaxWebSocketMessageBytes = 4 * 1024 * 1024;
inline constexpr qsizetype kMaxFramesPerDecode = 32;

struct FrameDecodeResult {
    QStringList frames;
    FrameError error = FrameError::None;
    bool hasMoreFrames = false;
};

QString escapeStt(const QString &value);
QString unescapeStt(const QString &value);
QString serializeStt(const QMap<QString, QString> &fields);
QMap<QString, QString> parseStt(const QString &raw);
QByteArray encodeFrame(const QString &payload, quint16 protocol = 689);

class FrameDecoder final {
public:
    FrameDecodeResult push(QByteArrayView chunk);
    FrameDecodeResult push(QByteArrayView chunk, qsizetype maxFrames);
    bool hasPending() const;
    qsizetype pendingBytes() const;
    void clear();

private:
    QByteArray pending_;
};

} // namespace DouyuDanmakuProtocol
