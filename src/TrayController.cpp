#include "TrayController.h"
#include "Utilities/Logger.h"

#include <cstring>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")

namespace rjo {

namespace {
constexpr UINT kTrayId = 1;
constexpr UINT kMaxMonitors = 16;

HICON CreateLeafTrayIcon() {
    const int size = 32;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC hdc = CreateCompatibleDC(screen);
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (hdc) DeleteDC(hdc);
        ReleaseDC(nullptr, screen);
        return LoadIconW(nullptr, IDI_APPLICATION);
    }

    HGDIOBJ oldBmp = SelectObject(hdc, dib);

    // Dark navy circle background
    RECT full{0, 0, size, size};
    HBRUSH bg = CreateSolidBrush(RGB(18, 16, 28));
    FillRect(hdc, &full, bg);
    DeleteObject(bg);

    HBRUSH disc = CreateSolidBrush(RGB(28, 24, 42));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(196, 90, 48));
    HGDIOBJ oldBrush = SelectObject(hdc, disc);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Ellipse(hdc, 1, 1, size - 1, size - 1);

    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    HGDIOBJ oldFont = SelectObject(hdc, font);

    // Amber / sakura glyphs
    SetTextColor(hdc, RGB(232, 140, 48));
    TextOutW(hdc, 4, 6, L"*", 1);
    SetTextColor(hdc, RGB(220, 90, 120));
    TextOutW(hdc, 15, 4, L"~", 1);
    SetTextColor(hdc, RGB(255, 180, 70));
    TextOutW(hdc, 10, 14, L"+", 1);

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(font);
    DeleteObject(pen);
    DeleteObject(disc);

    // Build alpha mask: make near-black outside circle transparent-ish for tray.
    auto* px = static_cast<uint32_t*>(bits);
    for (int i = 0; i < size * size; ++i) {
        const uint8_t b = static_cast<uint8_t>(px[i] & 0xFF);
        const uint8_t g = static_cast<uint8_t>((px[i] >> 8) & 0xFF);
        const uint8_t r = static_cast<uint8_t>((px[i] >> 16) & 0xFF);
        uint8_t a = 255;
        if (r < 22 && g < 20 && b < 32) {
            a = 0;
        }
        px[i] = (static_cast<uint32_t>(a) << 24) |
                (static_cast<uint32_t>(r) << 16) |
                (static_cast<uint32_t>(g) << 8) |
                b;
    }

    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = dib;
    ii.hbmMask = mask;
    HICON icon = CreateIconIndirect(&ii);

    SelectObject(hdc, oldBmp);
    DeleteObject(mask);
    DeleteObject(dib);
    DeleteDC(hdc);
    ReleaseDC(nullptr, screen);

    return icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
}

} // namespace

bool TrayController::Create(HINSTANCE instance, HWND messageWindow) {
    instance_ = instance;
    messageHwnd_ = messageWindow;

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = messageHwnd_;
    nid_.uID = kTrayId;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid_.uCallbackMessage = WM_TRAY;
    ownedIcon_ = CreateLeafTrayIcon();
    nid_.hIcon = ownedIcon_;
    wcsncpy_s(nid_.szTip, L"RetroJapanOverlay", _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &nid_)) {
        Logger::Instance().Error("Failed to add tray icon");
        return false;
    }

    nid_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    created_ = true;
    Logger::Instance().Info("Tray icon created");
    return true;
}

void TrayController::Destroy() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
    if (ownedIcon_) {
        DestroyIcon(ownedIcon_);
        ownedIcon_ = nullptr;
        nid_.hIcon = nullptr;
    }
}

void TrayController::UpdateTip(const std::wstring& tip) {
    if (!created_) return;
    wcsncpy_s(nid_.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayController::UpdateMenuContext(const AppConfig& config, const std::vector<std::wstring>& monitorNames) {
    lastConfig_ = config;
    lastMonitors_ = monitorNames;
}

bool TrayController::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAY) {
        OnTrayNotify(lParam);
        return true;
    }
    if (msg == WM_COMMAND) {
        const UINT id = LOWORD(wParam);
        // Only treat MonitorBase .. MonitorBase+N as monitor picks.
        // Palette IDs (1900+) must NOT fall into this branch.
        if (id >= static_cast<UINT>(TrayCommand::MonitorBase) &&
            id < static_cast<UINT>(TrayCommand::MonitorBase) + kMaxMonitors) {
            if (onCommand_) {
                onCommand_(TrayCommand::MonitorBase,
                           static_cast<int>(id - static_cast<UINT>(TrayCommand::MonitorBase)));
            }
            return true;
        }
        if (onCommand_) {
            onCommand_(static_cast<TrayCommand>(id), 0);
        }
        return true;
    }
    return false;
}

void TrayController::OnTrayNotify(LPARAM lParam) {
    switch (LOWORD(lParam)) {
    case WM_LBUTTONDBLCLK:
        if (onSettings_) {
            onSettings_();
        }
        break;
    case WM_LBUTTONUP:
        if (onCommand_) {
            onCommand_(TrayCommand::PauseResume, 0);
        }
        break;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        ShowContextMenu(lastConfig_, lastMonitors_);
        break;
    default:
        break;
    }
}

void TrayController::ShowContextMenu(const AppConfig& config, const std::vector<std::wstring>& monitorNames) {
    lastConfig_ = config;
    lastMonitors_ = monitorNames;

    HMENU menu = BuildMenu(config, monitorNames);
    if (!menu) {
        return;
    }

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(messageHwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, messageHwnd_, nullptr);
    PostMessageW(messageHwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

HMENU TrayController::BuildMenu(const AppConfig& config, const std::vector<std::wstring>& monitorNames) const {
    HMENU menu = CreatePopupMenu();
    if (!menu) return nullptr;

    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayCommand::PauseResume),
                config.paused ? L"Resume" : L"Pause");
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayCommand::EnableDisable),
                config.overlayEnabled ? L"Disable Overlay" : L"Enable Overlay");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    HMENU palette = CreatePopupMenu();
    auto palItem = [&](TrayCommand cmd, int value, const wchar_t* label) {
        UINT flags = MF_STRING;
        if (config.leafPalette == value) flags |= MF_CHECKED;
        AppendMenuW(palette, flags, static_cast<UINT>(cmd), label);
    };
    palItem(TrayCommand::PaletteAutumn, static_cast<int>(LeafPalette::Autumn), L"Autumn");
    palItem(TrayCommand::PaletteSakura, static_cast<int>(LeafPalette::Sakura), L"Sakura");
    palItem(TrayCommand::PaletteAmber, static_cast<int>(LeafPalette::Amber), L"Amber");
    palItem(TrayCommand::PaletteCyber, static_cast<int>(LeafPalette::Cyber), L"Cyber");
    palItem(TrayCommand::PaletteMidnight, static_cast<int>(LeafPalette::Midnight), L"Midnight");
    palItem(TrayCommand::PaletteMono, static_cast<int>(LeafPalette::Mono), L"Mono");
    palItem(TrayCommand::PaletteCrimson, static_cast<int>(LeafPalette::Crimson), L"Crimson");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(palette), L"Leaf Palette");

    HMENU density = CreatePopupMenu();
    AppendMenuW(density, MF_STRING, static_cast<UINT>(TrayCommand::DensityLow), L"Low (300)");
    AppendMenuW(density, MF_STRING, static_cast<UINT>(TrayCommand::DensityMed), L"Medium (900)");
    AppendMenuW(density, MF_STRING, static_cast<UINT>(TrayCommand::DensityHigh), L"High (1600)");
    AppendMenuW(density, MF_STRING, static_cast<UINT>(TrayCommand::DensityUltra), L"Ultra (3000)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(density), L"Leaf Density");

    HMENU reaction = CreatePopupMenu();
    AppendMenuW(reaction, MF_STRING, static_cast<UINT>(TrayCommand::ReactionLow), L"Gentle");
    AppendMenuW(reaction, MF_STRING, static_cast<UINT>(TrayCommand::ReactionMed), L"Normal");
    AppendMenuW(reaction, MF_STRING, static_cast<UINT>(TrayCommand::ReactionHigh), L"Strong");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(reaction), L"Leaf Reaction");

    HMENU wind = CreatePopupMenu();
    AppendMenuW(wind, MF_STRING, static_cast<UINT>(TrayCommand::WindLow), L"Calm");
    AppendMenuW(wind, MF_STRING, static_cast<UINT>(TrayCommand::WindMed), L"Breezy");
    AppendMenuW(wind, MF_STRING, static_cast<UINT>(TrayCommand::WindHigh), L"Windy");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(wind), L"Global Wind");

    HMENU speed = CreatePopupMenu();
    AppendMenuW(speed, MF_STRING, static_cast<UINT>(TrayCommand::SpeedSlow), L"Slow");
    AppendMenuW(speed, MF_STRING, static_cast<UINT>(TrayCommand::SpeedNormal), L"Normal");
    AppendMenuW(speed, MF_STRING, static_cast<UINT>(TrayCommand::SpeedFast), L"Fast");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(speed), L"Animation Speed");

    HMENU bright = CreatePopupMenu();
    AppendMenuW(bright, MF_STRING, static_cast<UINT>(TrayCommand::BrightLow), L"Dim");
    AppendMenuW(bright, MF_STRING, static_cast<UINT>(TrayCommand::BrightMed), L"Normal");
    AppendMenuW(bright, MF_STRING, static_cast<UINT>(TrayCommand::BrightHigh), L"Bright");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(bright), L"Scene Brightness");

    HMENU opacity = CreatePopupMenu();
    AppendMenuW(opacity, MF_STRING, static_cast<UINT>(TrayCommand::OpacityLow), L"Subtle");
    AppendMenuW(opacity, MF_STRING, static_cast<UINT>(TrayCommand::OpacityMed), L"Balanced");
    AppendMenuW(opacity, MF_STRING, static_cast<UINT>(TrayCommand::OpacityHigh), L"Strong");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(opacity), L"Overlay Opacity");

    UINT sceneFlags = MF_STRING | (config.showScene ? MF_CHECKED : 0);
    AppendMenuW(menu, sceneFlags, static_cast<UINT>(TrayCommand::ToggleScene), L"Show ASCII Scene");

    HMENU fps = CreatePopupMenu();
    auto fpsItem = [&](TrayCommand cmd, int value, const wchar_t* label) {
        UINT flags = MF_STRING;
        if (config.fpsLimit == value) flags |= MF_CHECKED;
        AppendMenuW(fps, flags, static_cast<UINT>(cmd), label);
    };
    fpsItem(TrayCommand::Fps30, 30, L"30 FPS");
    fpsItem(TrayCommand::Fps60, 60, L"60 FPS");
    fpsItem(TrayCommand::Fps120, 120, L"120 FPS");
    fpsItem(TrayCommand::Fps144, 144, L"144 FPS");
    fpsItem(TrayCommand::FpsUnlimited, 0, L"Unlimited");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fps), L"FPS Limit");

    HMENU monitors = CreatePopupMenu();
    UINT allFlags = MF_STRING | (config.monitorIndex < 0 ? MF_CHECKED : 0);
    AppendMenuW(monitors, allFlags, static_cast<UINT>(TrayCommand::MonitorAll), L"All Monitors");
    for (size_t i = 0; i < monitorNames.size() && i < kMaxMonitors; ++i) {
        UINT flags = MF_STRING;
        if (config.monitorIndex == static_cast<int>(i)) flags |= MF_CHECKED;
        std::wstring label = L"Monitor " + std::to_wstring(i + 1);
        if (!monitorNames[i].empty()) {
            label += L" (" + monitorNames[i] + L")";
        }
        AppendMenuW(monitors, flags,
                    static_cast<UINT>(TrayCommand::MonitorBase) + static_cast<UINT>(i),
                    label.c_str());
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(monitors), L"Monitor");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    UINT swFlags = MF_STRING | (config.startWithWindows ? MF_CHECKED : 0);
    AppendMenuW(menu, swFlags, static_cast<UINT>(TrayCommand::StartWithWindows), L"Start with Windows");
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayCommand::OpenSettings), L"Open Settings");
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayCommand::ReloadConfig), L"Reload Configuration");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayCommand::Exit), L"Exit");
    return menu;
}

} // namespace rjo
