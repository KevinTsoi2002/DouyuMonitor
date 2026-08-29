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
};

struct FrameDecodeResult {
    QStringList frames;
    FrameError error = FrameError::None;
};

QString escapeStt(const QString &value);
QString unescapeStt(const QString &value);
QString serializeStt(const QMap<QString, QString> &fields);
QMap<QString, QString> parseStt(const QString &raw);
QByteArray encodeFrame(const QString &payload, quint16 protocol = 689);

class FrameDecoder final {
public:
    FrameDecodeResult push(QByteArrayView chunk);
    void clear();

private:
    QByteArray pending_;
};

} // namespace DouyuDanmakuProtocol
