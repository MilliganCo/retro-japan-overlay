#include "Application.h"
#include "Utilities/Logger.h"
#include "Utilities/Paths.h"

#include <Windows.h>
#include <exception>
#include <string>

namespace {
constexpr wchar_t kMutexName[] = L"Local\\RetroJapanOverlay.SingleInstance";
constexpr wchar_t kWakeWindow[] = L"RetroJapanOverlay.MessageWindow";
constexpr UINT WM_APP_WAKE = WM_APP + 77;

bool SignalExistingInstance() {
    HWND hwnd = FindWindowW(kWakeWindow, L"RetroJapanOverlay.Message");
    if (hwnd) {
        PostMessageW(hwnd, WM_APP_WAKE, 0, 0);
        return true;
    }
    return false;
}
} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    // COM for DirectComposition / shell.
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE) {
        MessageBoxW(nullptr, L"Failed to initialize COM.", L"RetroJapanOverlay", MB_OK | MB_ICONERROR);
        return 1;
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex) {
        MessageBoxW(nullptr, L"Failed to create single-instance mutex.", L"RetroJapanOverlay",
                    MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        SignalExistingInstance();
        CloseHandle(mutex);
        CoUninitialize();
        return 0;
    }

    int exitCode = 1;
    try {
        rjo::EnsureAppDirectories();
        rjo::Logger::Instance().Initialize(rjo::GetLogDirectory());

        rjo::Application app;
        if (!app.Initialize(hInstance)) {
            MessageBoxW(nullptr, L"RetroJapanOverlay failed to start. See logs in %LOCALAPPDATA%\\RetroJapanOverlay\\logs\\",
                        L"RetroJapanOverlay", MB_OK | MB_ICONERROR);
        } else {
            exitCode = app.Run();
        }
    } catch (const std::exception& ex) {
        rjo::Logger::Instance().Error(std::string("Fatal: ") + ex.what());
        MessageBoxA(nullptr, ex.what(), "RetroJapanOverlay", MB_OK | MB_ICONERROR);
        exitCode = 1;
    } catch (...) {
        rjo::Logger::Instance().Error("Unknown fatal error");
        MessageBoxW(nullptr, L"Unknown fatal error.", L"RetroJapanOverlay", MB_OK | MB_ICONERROR);
        exitCode = 1;
    }

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    CoUninitialize();
    return exitCode;
}
