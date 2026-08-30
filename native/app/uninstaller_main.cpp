#include <windows.h>
#include <shlobj.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kProductDirectoryName[] = L"DouyuMonitor";
constexpr wchar_t kApplicationExecutableName[] = L"douyu_monitor_native.exe";
constexpr wchar_t kUninstallerExecutableName[] = L"Uninstall DouyuMonitor.exe";
constexpr wchar_t kProgramShortcutName[] = L"DouyuMonitor.lnk";
constexpr wchar_t kUninstallShortcutName[] = L"Uninstall DouyuMonitor.lnk";
constexpr wchar_t kUninstallRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\DouyuMonitor";

void showError(const wchar_t *message)
{
    MessageBoxW(nullptr, message, L"DouyuMonitor", MB_OK | MB_ICONERROR);
}

std::filesystem::path currentExecutablePath()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) return {};
        if (copied < buffer.size() - 1) return std::filesystem::path(std::wstring(buffer.data(), copied));
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path knownFolder(REFKNOWNFOLDERID folderId)
{
    PWSTR folderPath = nullptr;
    const HRESULT result = SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &folderPath);
    if (FAILED(result) || folderPath == nullptr) return {};

    const std::filesystem::path resultPath(folderPath);
    CoTaskMemFree(folderPath);
    return resultPath;
}

bool isInstallDirectory(const std::filesystem::path &directory)
{
    return directory.filename() == kProductDirectoryName
        && directory.has_parent_path()
        && std::filesystem::exists(directory / kApplicationExecutableName)
        && std::filesystem::exists(directory / kUninstallerExecutableName);
}

void removeUserEntryPoints()
{
    std::error_code error;
    const auto desktopDirectory = knownFolder(FOLDERID_Desktop);
    if (!desktopDirectory.empty()) {
        std::filesystem::remove(desktopDirectory / kProgramShortcutName, error);
    }

    const auto startMenuDirectory = knownFolder(FOLDERID_Programs);
    if (!startMenuDirectory.empty()) {
        const auto productDirectory = startMenuDirectory / kProductDirectoryName;
        std::filesystem::remove(productDirectory / kProgramShortcutName, error);
        std::filesystem::remove(productDirectory / kUninstallShortcutName, error);
        std::filesystem::remove(productDirectory, error);
    }

    RegDeleteTreeW(HKEY_CURRENT_USER, kUninstallRegistryKey);
}

bool scheduleRemoval(const std::filesystem::path &installDirectory)
{
    const std::wstring quotedDirectory = L"\"" + installDirectory.wstring() + L"\"";
    std::wstring command = L"cmd.exe /d /s /c \"ping 127.0.0.1 -n 3 > nul & rmdir /s /q "
        + quotedDirectory + L"\"";
    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION processInfo{};
    const BOOL started = CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);
    if (started == FALSE) return false;

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    const auto executablePath = currentExecutablePath();
    const auto installDirectory = executablePath.parent_path();
    if (executablePath.filename() != kUninstallerExecutableName || !isInstallDirectory(installDirectory)) {
        showError(L"The uninstaller must be run from a valid DouyuMonitor installation folder.");
        return 1;
    }

    const int confirmation = MessageBoxW(nullptr,
        L"Uninstall DouyuMonitor and remove its current-user shortcuts?",
        L"Uninstall DouyuMonitor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (confirmation != IDYES) return 0;

    removeUserEntryPoints();
    if (!scheduleRemoval(installDirectory)) {
        showError(L"DouyuMonitor could not schedule removal of its installation folder.");
        return 1;
    }

    return 0;
}
