#include "app/windows_tray_service.h"

#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>

#include <memory>

namespace {

constexpr UINT kTrayMessage = WM_APP + 41;
constexpr UINT kShowCommand = 1001;
constexpr UINT kQuitCommand = 1002;
constexpr UINT kTrayIconId = 1;
constexpr wchar_t kWindowClassName[] = L"DouyuMonitorTrayMessageWindow";

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

class WindowsTrayService::Private {
public:
    WindowsTrayService *owner = nullptr;
    QWindow *window = nullptr;
    HWND messageWindow = nullptr;
    NOTIFYICONDATAW icon{};
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

WindowsTrayService::WindowsTrayService(QObject *parent)
    : QObject(parent), d_(new Private)
{
    d_->owner = this;
}

WindowsTrayService::~WindowsTrayService()
{
    stop();
    delete d_;
}

bool WindowsTrayService::start(QWindow *window)
{
    if (d_->messageWindow != nullptr) return true;
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

    d_->icon = {};
    d_->icon.cbSize = sizeof(d_->icon);
    d_->icon.hWnd = d_->messageWindow;
    d_->icon.uID = kTrayIconId;
    d_->icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    d_->icon.uCallbackMessage = kTrayMessage;
    d_->icon.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    if (d_->icon.hIcon == nullptr) d_->icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(d_->icon.szTip, L"DouyuMonitor");
    if (!Shell_NotifyIconW(NIM_ADD, &d_->icon)) {
        DestroyWindow(d_->messageWindow);
        d_->messageWindow = nullptr;
        return false;
    }
    return true;
}

void WindowsTrayService::stop()
{
    if (d_->messageWindow == nullptr) return;
    Shell_NotifyIconW(NIM_DELETE, &d_->icon);
    DestroyWindow(d_->messageWindow);
    d_->messageWindow = nullptr;
    d_->window = nullptr;
}

bool WindowsTrayService::isRunning() const noexcept
{
    return d_->messageWindow != nullptr;
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

WindowsTrayService::WindowsTrayService(QObject *parent)
    : QObject(parent), d_(new Private)
{
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
