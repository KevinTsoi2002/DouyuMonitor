#include "media/media_source.h"

#include <QRegularExpression>

namespace {

const QRegularExpression kUriSchemePattern(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9+.-]*:)"));
const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));
const QStringList kAllowedHostSuffixes = {
    QStringLiteral(".douyucdn.cn"),
    QStringLiteral(".douyucdn2.cn"),
    QStringLiteral(".edgesrv.com"),
};

bool isAllowedRemoteUrl(const QUrl &url)
{
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))
        || url.host().isEmpty() || !url.userName().isEmpty() || !url.password().isEmpty()) {
        return false;
    }

    const QString host = url.host().toLower();
    for (const QString &suffix : kAllowedHostSuffixes) {
        if (host.endsWith(suffix)) return true;
    }
    return false;
}

} // namespace

std::optional<MediaSource> MediaSource::fromDescriptor(const QString &descriptor)
{
    const QString normalized = descriptor.trimmed();
    const bool looksLikeWindowsDrivePath = normalized.size() >= 3
        && normalized.at(1) == QLatin1Char(':')
        && (normalized.at(2) == QLatin1Char('/') || normalized.at(2) == QLatin1Char('\\'));
    if (normalized.isEmpty()
        || (!looksLikeWindowsDrivePath && kUriSchemePattern.match(normalized).hasMatch())) {
        return std::nullopt;
    }

    return MediaSource(normalized);
}

std::optional<MediaSource> MediaSource::fromRemoteVariant(const QString &roomId,
                                                           const StreamVariant &variant)
{
    if (!kRoomIdPattern.match(roomId).hasMatch() || variant.id.isEmpty()
        || variant.container.isEmpty() || !isAllowedRemoteUrl(variant.playbackUrl)) {
        return std::nullopt;
    }

    return MediaSource(roomId, variant);
}

MediaSource::Kind MediaSource::kind() const noexcept
{
    return kind_;
}

QString MediaSource::localPath() const
{
    return localPath_;
}

QUrl MediaSource::remoteUrl() const
{
    return remoteUrl_;
}

QString MediaSource::roomId() const
{
    return roomId_;
}

QString MediaSource::variantId() const
{
    return variantId_;
}

StreamQuality MediaSource::quality() const noexcept
{
    return quality_;
}

QString MediaSource::container() const
{
    return container_;
}

QString MediaSource::stableDescription() const
{
    return kind_ == Kind::RemoteStream ? QStringLiteral("remote-stream")
                                       : QStringLiteral("local-file");
}

MediaSource::MediaSource(QString localPath)
    : localPath_(std::move(localPath))
{
}

MediaSource::MediaSource(QString roomId, StreamVariant variant)
    : kind_(Kind::RemoteStream)
    , remoteUrl_(std::move(variant.playbackUrl))
    , roomId_(std::move(roomId))
    , variantId_(std::move(variant.id))
    , quality_(variant.quality)
    , container_(std::move(variant.container))
{
}
