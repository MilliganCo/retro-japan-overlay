#include "Utilities/Paths.h"

#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace rjo {

namespace {

std::wstring LocalAppData() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        std::wstring result(path);
        CoTaskMemFree(path);
        return result;
    }
    return L".";
}

} // namespace

std::wstring GetAppDataRoot() {
    return (fs::path(LocalAppData()) / L"RetroJapanOverlay").wstring();
}

std::wstring GetConfigPath() {
    return (fs::path(GetAppDataRoot()) / L"config.ini").wstring();
}

std::wstring GetLogDirectory() {
    return (fs::path(GetAppDataRoot()) / L"logs").wstring();
}

std::wstring GetDefaultConfigTemplatePath() {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    fs::path exeDir = fs::path(modulePath).parent_path();
    return (exeDir / L"default_config.ini").wstring();
}

bool EnsureAppDirectories() {
    std::error_code ec;
    fs::create_directories(GetAppDataRoot(), ec);
    fs::create_directories(GetLogDirectory(), ec);
    return !ec;
}

} // namespace rjo
