#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

enum class DanmakuConnectionState {
    Idle,
    Connecting,
    Connected,
    Reconnecting,
    Failed,
    PlatformBlocked,
};

enum class DanmakuErrorCode {
    None,
    NetworkUnavailable,
    HandshakeTimeout,
    ProtocolChanged,
    RetryExhausted,
    AuthRequired,
};

enum class DanmakuRegion {
    Full,
    Top,
    Bottom,
};

enum class DanmakuDensity {
    Massive,
    Normal,
    Reduced,
};

enum class DanmakuFontFamily {
    SimHei,
    MicrosoftYaHei,
};

enum class DanmakuRendering {
    Native,
    Advanced,
};

struct DanmakuMessage {
    QString id;
    QString roomId;
    QString nickname;
    QString text;
    QDateTime receivedAtUtc;
};

struct DanmakuConnectionStatus {
    QString roomId;
    DanmakuConnectionState state = DanmakuConnectionState::Idle;
    int attempt = 0;
    DanmakuErrorCode errorCode = DanmakuErrorCode::None;
};

struct DanmakuDisplaySettings {
    int durationSeconds = 8;
    int fontSize = 24;
    qreal opacity = 0.9;
    DanmakuRegion region = DanmakuRegion::Full;
    DanmakuDensity density = DanmakuDensity::Normal;
    DanmakuFontFamily fontFamily = DanmakuFontFamily::MicrosoftYaHei;
    DanmakuRendering rendering = DanmakuRendering::Native;

    bool operator==(const DanmakuDisplaySettings &) const = default;
};

struct DanmakuGovernanceSettings {
    bool enabled = true;
    QStringList keywordBlacklist;
    int duplicateWindowSeconds = 3;
    bool peakProtectionEnabled = true;

    bool operator==(const DanmakuGovernanceSettings &) const = default;
};

struct DanmakuGovernanceOverride {
    std::optional<bool> enabled;
    std::optional<QStringList> keywordBlacklist;
    std::optional<int> duplicateWindowSeconds;
    std::optional<bool> peakProtectionEnabled;

    bool operator==(const DanmakuGovernanceOverride &) const = default;
};

struct DanmakuGovernanceStats {
    QString level = QStringLiteral("normal");
    qreal recentRate = 0;
    qreal peakRate = 0;
    int filtered = 0;
    int duplicates = 0;
    int rateLimited = 0;
    int queueOverflow = 0;
    int upstreamDropped = 0;
};
