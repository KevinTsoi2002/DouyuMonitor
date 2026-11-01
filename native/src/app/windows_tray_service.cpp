#include "app/windows_tray_service.h"

#include <QWindow>

#include "app/windows_notification_service.h"

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <string>

#include <memory>

namespace {

constexpr UINT kTrayMessage = WM_APP + 41;
constexpr UINT kShowCommand = 1001;
constexpr UINT kQuitCommand = 1002;
constexpr UINT kTrayIconId = 1;
constexpr wchar_t kWindowClassName[] = L"DouyuMonitorTrayMessageWindow";

template <size_t Size>
void copy(wchar_t (&destination)[Size], const QString &source)
{
    const std::wstring value = source.toStdWString();
    wcsncpy_s(destination, value.c_str(), _TRUNCATE);
}

class Win32TrayShellApi final : public WindowsTrayShellApi {
public:
    bool add(const IconSpec &icon) override
    {
        NOTIFYICONDATAW data{};
        if (!fillIdentity(data, icon.identity)) return false;
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data.uCallbackMessage = static_cast<UINT>(icon.callbackMessage);
        data.hIcon = reinterpret_cast<HICON>(icon.iconHandle);
        if (data.hIcon == nullptr) return false;
        copy(data.szTip, icon.tooltip);
        return Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    }

    bool modify(const IconIdentity &identity,
                const NotificationSpec &notification) override
    {
        NOTIFYICONDATAW data{};
        if (!fillIdentity(data, identity)) return false;
        data.uFlags = static_cast<UINT>(notification.flags);
        data.dwInfoFlags = static_cast<DWORD>(notification.infoFlags);
        data.uTimeout = static_cast<UINT>(notification.timeoutMs);
        copy(data.szInfoTitle, notification.title);
        copy(data.szInfo, notification.body);
        return Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
    }

    bool remove(const IconIdentity &identity) override
    {
        NOTIFYICONDATAW data{};
        if (!fillIdentity(data, identity)) return false;
        return Shell_NotifyIconW(NIM_DELETE, &data) != FALSE;
    }

private:
    static bool fillIdentity(NOTIFYICONDATAW &data, const IconIdentity &identity)
    {
        if (identity.window == 0 || identity.id == 0) return false;
        data.cbSize = sizeof(data);
        data.hWnd = reinterpret_cast<HWND>(identity.window);
        data.uID = static_cast<UINT>(identity.id);
        return true;
    }
};

ATOM ensureWindowClass(HINSTANCE instance, WNDPROC procedure)
{
    static ATOM atom = [] { return ATOM{}; }();
    if (atom != 0) return atom;

    WNDCLASSEXW klass{};
    klass.cbSize = sizeof(klass);
    klass.lpfnWndProc = procedure;
    klass.hInstance = instance;
    klass.lpszClassName = kWindowClassName;
    atom = RegisterClassExW(&klass);
    if (atom == 0 && GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        atom = 1;
    }
    return atom;
}

} // namespace

class WindowsTrayService::Private : public SystemNotificationSink {
public:
    bool available() const override
    {
        return iconAdded;
    }

    bool show(const QString &title, const QString &body) override
    {
        return owner != nullptr && owner->showNotification(title, body);
    }

    WindowsTrayService *owner = nullptr;
    QWindow *window = nullptr;
    HWND messageWindow = nullptr;
    std::unique_ptr<WindowsTrayShellApi> shellApi;
    WindowsTrayShellApi::IconIdentity identity;
    WindowsTrayShellApi::IconSpec icon;
    bool iconAdded = false;
};

static LRESULT CALLBACK trayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *state = reinterpret_cast<WindowsTrayService::Private *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        state = static_cast<WindowsTrayService::Private *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    if (state != nullptr) {
        if (message == kTrayMessage) {
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
                emit state->owner->showRequested();
                return 0;
            }
            if (lParam == WM_RBUTTONUP) {
                HMENU menu = CreatePopupMenu();
                if (menu == nullptr) return 0;
                AppendMenuW(menu, MF_STRING, kShowCommand, L"显示窗口");
                AppendMenuW(menu, MF_STRING, kQuitCommand, L"退出程序");
                POINT point{};
                GetCursorPos(&point);
                SetForegroundWindow(hwnd);
                const UINT command = TrackPopupMenu(menu,
                                                    TPM_RETURNCMD | TPM_NONOTIFY,
                                                    point.x, point.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                if (command == kShowCommand) emit state->owner->showRequested();
                if (command == kQuitCommand) emit state->owner->quitRequested();
                return 0;
            }
        }
        if (message == WM_COMMAND) {
            if (LOWORD(wParam) == kShowCommand) emit state->owner->showRequested();
            if (LOWORD(wParam) == kQuitCommand) emit state->owner->quitRequested();
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

WindowsTrayService::WindowsTrayService(QObject *parent,
                                       std::unique_ptr<WindowsTrayShellApi> shellApi)
    : QObject(parent), d_(new Private)
{
    d_->owner = this;
    d_->shellApi = std::move(shellApi);
    if (d_->shellApi == nullptr) d_->shellApi = std::make_unique<Win32TrayShellApi>();
}

WindowsTrayService::~WindowsTrayService()
{
    stop();
    delete d_;
}

bool WindowsTrayService::start(QWindow *window)
{
    if (d_->iconAdded) return true;
    if (d_->shellApi == nullptr) return false;
    d_->window = window;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    if (ensureWindowClass(instance, trayWindowProc) == 0) return false;
    d_->messageWindow = CreateWindowExW(0,
                                        kWindowClassName,
                                        L"DouyuMonitorTray",
                                        0,
                                        0,
                                        0,
                                        0,
                                        0,
                                        HWND_MESSAGE,
                                        nullptr,
                                        instance,
                                        d_);
    if (d_->messageWindow == nullptr) return false;

    d_->identity = {reinterpret_cast<quintptr>(d_->messageWindow), kTrayIconId};
    d_->icon = {
        d_->identity,
        kTrayMessage,
        reinterpret_cast<quintptr>(LoadIconW(instance, MAKEINTRESOURCEW(101))),
        QStringLiteral("DouyuMonitor"),
    };
    if (d_->icon.iconHandle == 0) {
        d_->icon.iconHandle =
            reinterpret_cast<quintptr>(LoadIconW(nullptr, IDI_APPLICATION));
    }
    if (!d_->shellApi->add(d_->icon)) {
        DestroyWindow(d_->messageWindow);
        d_->messageWindow = nullptr;
        d_->window = nullptr;
        d_->identity = {};
        d_->icon = {};
        return false;
    }
    d_->iconAdded = true;
    return true;
}

void WindowsTrayService::stop()
{
    if (!d_->iconAdded && d_->messageWindow == nullptr) return;
    if (d_->iconAdded && d_->shellApi != nullptr) {
        d_->shellApi->remove(d_->identity);
    }
    if (d_->messageWindow != nullptr) DestroyWindow(d_->messageWindow);
    d_->iconAdded = false;
    d_->messageWindow = nullptr;
    d_->window = nullptr;
    d_->identity = {};
    d_->icon = {};
}

bool WindowsTrayService::showNotification(const QString &title, const QString &body)
{
    if (!d_->iconAdded || d_->shellApi == nullptr) return false;
    return d_->shellApi->modify(
        d_->identity,
        {title, body, NIF_INFO, NIIF_INFO, 5000});
}

bool WindowsTrayService::isRunning() const noexcept
{
    return d_->iconAdded;
}

SystemNotificationSink *WindowsTrayService::notificationSink() noexcept
{
    return d_;
}

#ifdef DOUYU_TESTING
void WindowsTrayService::triggerShowForTest()
{
    emit showRequested();
}

void WindowsTrayService::triggerQuitForTest()
{
    emit quitRequested();
}
#endif

#else

class WindowsTrayService::Private {};

WindowsTrayService::WindowsTrayService(QObject *parent,
                                       std::unique_ptr<WindowsTrayShellApi> shellApi)
    : QObject(parent), d_(new Private)
{
    Q_UNUSED(shellApi);
}

WindowsTrayService::~WindowsTrayService()
{
    delete d_;
}

bool WindowsTrayService::start(QWindow *)
{
    return false;
}

void WindowsTrayService::stop()
{
}

bool WindowsTrayService::isRunning() const noexcept
{
    return false;
}

SystemNotificationSink *WindowsTrayService::notificationSink() noexcept
{
    return nullptr;
}

#ifdef DOUYU_TESTING
void WindowsTrayService::triggerShowForTest()
{
    emit showRequested();
}

void WindowsTrayService::triggerQuitForTest()
{
    emit quitRequested();
}
#endif

#endif
