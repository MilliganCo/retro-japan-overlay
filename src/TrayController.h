#pragma once

#include "Configuration.h"

#include <Windows.h>
#include <shellapi.h>
#include <functional>
#include <string>
#include <vector>

namespace rjo {

enum class TrayCommand : UINT {
    PauseResume = 1001,
    EnableDisable = 1002,
    OpenSettings = 1003,
    ReloadConfig = 1004,
    Exit = 1005,
    StartWithWindows = 1006,
    Fps30 = 1100,
    Fps60 = 1101,
    Fps120 = 1102,
    Fps144 = 1103,
    FpsUnlimited = 1104,
    DensityLow = 1200,
    DensityMed = 1201,
    DensityHigh = 1202,
    DensityUltra = 1203,
    ReactionLow = 1300,
    ReactionMed = 1301,
    ReactionHigh = 1302,
    WindLow = 1400,
    WindMed = 1401,
    WindHigh = 1402,
    SpeedSlow = 1500,
    SpeedNormal = 1501,
    SpeedFast = 1502,
    BrightLow = 1600,
    BrightMed = 1601,
    BrightHigh = 1602,
    OpacityLow = 1700,
    OpacityMed = 1701,
    OpacityHigh = 1702,
    MonitorAll = 1800,
    MonitorBase = 1801, // MonitorBase + index
    PaletteAutumn = 1900,
    PaletteSakura = 1901,
    PaletteAmber = 1902,
    PaletteCyber = 1903,
    PaletteMidnight = 1904,
    PaletteMono = 1905,
    PaletteCrimson = 1906,
    ToggleScene = 1910
};

class TrayController {
public:
    using CommandHandler = std::function<void(TrayCommand, int)>;
    using SettingsHandler = std::function<void()>;

    bool Create(HINSTANCE instance, HWND messageWindow);
    void Destroy();
    void UpdateTip(const std::wstring& tip);
    void UpdateMenuContext(const AppConfig& config, const std::vector<std::wstring>& monitorNames);
    void ShowContextMenu(const AppConfig& config, const std::vector<std::wstring>& monitorNames);

    void SetCommandHandler(CommandHandler handler) { onCommand_ = std::move(handler); }
    void SetSettingsHandler(SettingsHandler handler) { onSettings_ = std::move(handler); }

    static constexpr UINT WM_TRAY = WM_APP + 40;
    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void OnTrayNotify(LPARAM lParam);
    HMENU BuildMenu(const AppConfig& config, const std::vector<std::wstring>& monitorNames) const;

    NOTIFYICONDATAW nid_{};
    HWND messageHwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HICON ownedIcon_ = nullptr;
    bool created_ = false;
    CommandHandler onCommand_;
    SettingsHandler onSettings_;
    AppConfig lastConfig_{};
    std::vector<std::wstring> lastMonitors_;
};

} // namespace rjo
