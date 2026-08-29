#include "app/windows_notification_service.h"

#include <QSettings>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

class Win32NotificationSink final : public SystemNotificationSink {
public:
    Win32NotificationSink()
    {
#ifdef Q_OS_WIN
        instance_ = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = kWindowClass;
        const ATOM atom = RegisterClassW(&windowClass);
        if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

        window_ = CreateWindowExW(0, kWindowClass, L"DouyuMonitorNotification",
                                  0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance_, nullptr);
        if (window_ == nullptr) return;

        NOTIFYICONDATAW icon{};
        icon.cbSize = sizeof(icon);
        icon.hWnd = window_;
        icon.uID = 1;
        icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        icon.uCallbackMessage = WM_APP + 1;
        icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        copy(icon.szTip, L"Douyu Monitor");
        iconAdded_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
#endif
    }

    ~Win32NotificationSink() override
    {
#ifdef Q_OS_WIN
        if (iconAdded_) {
            NOTIFYICONDATAW icon{};
            icon.cbSize = sizeof(icon);
            icon.hWnd = window_;
            icon.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &icon);
        }
        if (window_ != nullptr) DestroyWindow(window_);
#endif
    }

    bool available() const override
    {
        return iconAdded_;
    }

    void show(const QString &, const QString &) override
    {
#ifdef Q_OS_WIN
        if (!available()) return;
        NOTIFYICONDATAW icon{};
        icon.cbSize = sizeof(icon);
        icon.hWnd = window_;
        icon.uID = 1;
        icon.uFlags = NIF_INFO;
        icon.dwInfoFlags = NIIF_INFO;
        icon.uTimeout = 5000;
        copy(icon.szInfoTitle, L"斗鱼监控");
        copy(icon.szInfo, L"房间状态已更新");
        Shell_NotifyIconW(NIM_MODIFY, &icon);
#endif
    }

private:
    template <size_t Size>
    static void copy(wchar_t (&destination)[Size], const wchar_t *source)
    {
#ifdef Q_OS_WIN
        wcsncpy_s(destination, source, _TRUNCATE);
#else
        Q_UNUSED(destination);
        Q_UNUSED(source);
#endif
    }

#ifdef Q_OS_WIN
    static constexpr wchar_t kWindowClass[] = L"DouyuMonitorNotificationWindow";
    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
#endif
    bool iconAdded_ = false;
};

QString key(NotificationEventType type)
{
    switch (type) {
    case NotificationEventType::RoomOnline:
        return QStringLiteral("roomOnline");
    case NotificationEventType::RoomOffline:
        return QStringLiteral("roomOffline");
    case NotificationEventType::PlaybackFailed:
        return QStringLiteral("playbackFailed");
    case NotificationEventType::PlaybackRecovered:
        return QStringLiteral("playbackRecovered");
    }
    return {};
}

} // namespace

WindowsNotificationService::WindowsNotificationService(QSettings *settings,
                                                       SystemNotificationSink *sink,
                                                       QObject *parent)
    : QObject(parent)
    , settings_(settings)
    , sink_(sink)
{
    loadPreferences();
    if (sink_ == nullptr) {
        ownedSink_ = std::make_unique<Win32NotificationSink>();
        sink_ = ownedSink_.get();
    }
}

WindowsNotificationService::~WindowsNotificationService() = default;

NotificationPreferences WindowsNotificationService::preferences() const noexcept
{
    return preferences_;
}

bool WindowsNotificationService::setPreferences(NotificationPreferences preferences)
{
    preferences_ = preferences;
    if (settings_ == nullptr) {
        statusText_.clear();
        return true;
    }

    settings_->setValue(QStringLiteral("notifications/enabled"), preferences_.enabled);
    settings_->setValue(QStringLiteral("notifications/roomOnline"), preferences_.roomOnline);
    settings_->setValue(QStringLiteral("notifications/roomOffline"), preferences_.roomOffline);
    settings_->setValue(QStringLiteral("notifications/playbackFailed"),
                        preferences_.playbackFailed);
    settings_->setValue(QStringLiteral("notifications/playbackRecovered"),
                        preferences_.playbackRecovered);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        statusText_ = QStringLiteral("通知设置保存失败");
        return false;
    }
    statusText_.clear();
    return true;
}

bool WindowsNotificationService::deliver(const NotificationEvent &event)
{
    if (!preferences_.enabled || !isEnabled(event.type) || sink_ == nullptr
        || !sink_->available()) {
        return false;
    }
    sink_->show(event.title, event.body);
    return true;
}

QString WindowsNotificationService::statusText() const
{
    return statusText_;
}

void WindowsNotificationService::loadPreferences()
{
    if (settings_ == nullptr) return;
    preferences_.enabled =
        settings_->value(QStringLiteral("notifications/enabled"), true).toBool();
    preferences_.roomOnline =
        settings_->value(QStringLiteral("notifications/roomOnline"), true).toBool();
    preferences_.roomOffline =
        settings_->value(QStringLiteral("notifications/roomOffline"), true).toBool();
    preferences_.playbackFailed =
        settings_->value(QStringLiteral("notifications/playbackFailed"), true).toBool();
    preferences_.playbackRecovered =
        settings_->value(QStringLiteral("notifications/playbackRecovered"), true).toBool();
}

bool WindowsNotificationService::isEnabled(NotificationEventType type) const noexcept
{
    switch (type) {
    case NotificationEventType::RoomOnline:
        return preferences_.roomOnline;
    case NotificationEventType::RoomOffline:
        return preferences_.roomOffline;
    case NotificationEventType::PlaybackFailed:
        return preferences_.playbackFailed;
    case NotificationEventType::PlaybackRecovered:
        return preferences_.playbackRecovered;
    }
    return false;
}
