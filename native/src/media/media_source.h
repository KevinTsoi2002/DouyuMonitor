#pragma once

#include <QUrl>
#include <QString>

#include <optional>

#include "service/stream_service_protocol.h"

class MediaSource final {
public:
    enum class Kind {
        LocalFile,
        RemoteStream,
    };

    static std::optional<MediaSource> fromDescriptor(const QString &descriptor);
    static std::optional<MediaSource> fromRemoteVariant(const QString &roomId,
                                                        const StreamVariant &variant);

    Kind kind() const noexcept;
    QString localPath() const;
    QUrl remoteUrl() const;
    QString roomId() const;
    QString variantId() const;
    StreamQuality quality() const noexcept;
    QString container() const;
    QString stableDescription() const;

private:
    explicit MediaSource(QString localPath);
    MediaSource(QString roomId, StreamVariant variant);

    Kind kind_ = Kind::LocalFile;
    QString localPath_;
    QUrl remoteUrl_;
    QString roomId_;
    QString variantId_;
    StreamQuality quality_ = StreamQuality::Auto;
    QString container_;
};

Q_DECLARE_METATYPE(MediaSource)
