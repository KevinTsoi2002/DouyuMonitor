#pragma once

#include "danmaku/danmaku_session_manager.h"
#include "workspace/native_workspace_types.h"

#include <QObject>
#include <QVariantMap>

class DanmakuController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool globalEnabled READ globalEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap displaySettings READ displaySettings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap governanceSettings READ governanceSettings NOTIFY settingsChanged)
    Q_PROPERTY(bool presentationSuspended READ presentationSuspended NOTIFY presentationSuspendedChanged)

public:
    explicit DanmakuController(DanmakuClientFactory factory = {}, QObject *parent = nullptr);

    bool globalEnabled() const noexcept;
    QVariantMap displaySettings() const;
    QVariantMap governanceSettings() const;
    const NativeDanmakuConfiguration &configuration() const noexcept;
    bool presentationSuspended() const noexcept;
    void setPresentationSuspended(bool suspended);

    void setConfiguration(const NativeDanmakuConfiguration &configuration);
    void synchronize(const QVector<DanmakuRoomEligibility> &rooms);
    void stopAll();

    Q_INVOKABLE void setGlobalEnabled(bool enabled);
    Q_INVOKABLE void setDisplaySetting(const QString &key, const QVariant &value);
    Q_INVOKABLE void setGovernanceSetting(const QString &roomId,
                                          const QString &key,
                                          const QVariant &value);
    Q_INVOKABLE void clearRoomGovernanceOverride(const QString &roomId);
    Q_INVOKABLE QVariantMap takeNextMessage(const QString &roomId);
    Q_INVOKABLE QVariantMap statusForRoom(const QString &roomId) const;
    Q_INVOKABLE QVariantMap statsForRoom(const QString &roomId) const;
    Q_INVOKABLE void clearStats(const QString &roomId);
    Q_INVOKABLE void retry(const QString &roomId);
    Q_INVOKABLE void clearRoom(const QString &roomId);

#ifdef DOUYU_TESTING
    int activeSessionCountForTest() const;
#endif

signals:
    void settingsChanged();
    void roomStateChanged(const QString &roomId);
    void messageAvailable(const QString &roomId);
    void presentationSuspendedChanged();

private:
    static DanmakuGovernanceOverride overrideWithSetting(
        DanmakuGovernanceOverride current, const QString &key, const QVariant &value);
    static void applyGovernanceSetting(DanmakuGovernanceSettings &settings,
                                       const QString &key,
                                       const QVariant &value);
    void emitSettingsIfChanged(const NativeDanmakuConfiguration &previous);

    NativeDanmakuConfiguration configuration_;
    DanmakuSessionManager sessions_;
    bool presentationSuspended_ = false;
};
