#include "SettingsWindow.h"
#include "Utilities/Logger.h"

#include <CommCtrl.h>
#include <algorithm>
#include <cmath>
#include <string>

#pragma comment(lib, "Comctl32.lib")

namespace rjo {

namespace {
constexpr wchar_t kSettingsClass[] = L"RetroJapanOverlay.SettingsWindow";
constexpr wchar_t kViewportClass[] = L"RetroJapanOverlay.SettingsViewport";
constexpr wchar_t kContentClass[]  = L"RetroJapanOverlay.SettingsContent";

constexpr COLORREF kBg     = RGB(18, 16, 28);
constexpr COLORREF kPanel  = RGB(28, 24, 40);
constexpr COLORREF kText   = RGB(232, 210, 190);
constexpr COLORREF kEditBg = RGB(36, 32, 52);

enum ControlId : int {
    IDC_OPACITY = 2001,
    IDC_SCENE_OPACITY,
    IDC_LEAF_OPACITY,
    IDC_LEAF_COUNT,
    IDC_LEAF_SIZE,
    IDC_GRAVITY,
    IDC_DRAG,
    IDC_WIND,
    IDC_TURBULENCE,
    IDC_CURSOR_RADIUS,
    IDC_CURSOR_REPULSION,
    IDC_CURSOR_VORTEX,
    IDC_CURSOR_WAKE,
    IDC_CURSOR_SMOOTH,
    IDC_ASCII_SIZE,
    IDC_CRT,
    IDC_SCANLINE,
    IDC_FPS,
    IDC_MONITOR,
    IDC_PALETTE,
    IDC_ENABLED,
    IDC_SHOW_SCENE,
    IDC_APPLY,
    IDC_RESET,
    IDC_CLOSE
};

int TrackPos(HWND tb) {
    return static_cast<int>(SendMessageW(tb, TBM_GETPOS, 0, 0));
}

void SetTrackPos(HWND tb, int pos) {
    SendMessageW(tb, TBM_SETPOS, TRUE, pos);
}

} // namespace

bool SettingsWindow::Create(HINSTANCE instance, HWND parentMessageHwnd) {
    instance_ = instance;
    parent_ = parentMessageHwnd;

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    bgBrush_ = CreateSolidBrush(kBg);
    editBrush_ = CreateSolidBrush(kEditBg);
    panelBrush_ = CreateSolidBrush(kPanel);
    font_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SettingsWindow::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kSettingsClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = bgBrush_;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW vc{};
    vc.cbSize = sizeof(vc);
    vc.lpfnWndProc = DefWindowProcW;
    vc.hInstance = instance;
    vc.lpszClassName = kViewportClass;
    vc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    vc.hbrBackground = bgBrush_;
    vc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&vc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW cc{};
    cc.cbSize = sizeof(cc);
    cc.lpfnWndProc = SettingsWindow::ContentProc;
    cc.hInstance = instance;
    cc.lpszClassName = kContentClass;
    cc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    cc.hbrBackground = bgBrush_;
    if (!RegisterClassExW(&cc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kSettingsClass,
        L"RetroJapanOverlay — Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_VSCROLL | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 640,
        nullptr, nullptr, instance, this);

    if (!hwnd_) {
        Logger::Instance().Error("Failed to create settings window");
        return false;
    }

    BuildControls();
    Relayout();
    return true;
}

void SettingsWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        viewport_ = nullptr;
        content_ = nullptr;
    }
    if (font_) { DeleteObject(font_); font_ = nullptr; }
    if (bgBrush_) { DeleteObject(bgBrush_); bgBrush_ = nullptr; }
    if (editBrush_) { DeleteObject(editBrush_); editBrush_ = nullptr; }
    if (panelBrush_) { DeleteObject(panelBrush_); panelBrush_ = nullptr; }
}

void SettingsWindow::Show(const AppConfig& config) {
    config_ = config;
    SyncFromConfig();
    scrollPos_ = 0;
    Relayout();
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void SettingsWindow::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

bool SettingsWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::ContentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        return self->HandleContentMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND SettingsWindow::AddLabel(int x, int y, int w, int h, const wchar_t* text) {
    HWND ctl = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                               x, y, w, h, content_, nullptr, instance_, nullptr);
    if (font_) SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return ctl;
}

HWND SettingsWindow::AddTrackbar(int id, int x, int y, int w, int h, int minV, int maxV, int pos) {
    HWND tb = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_TOOLTIPS,
                              x, y, w, h, content_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              instance_, nullptr);
    SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(minV, maxV));
    SendMessageW(tb, TBM_SETPOS, TRUE, pos);
    return tb;
}

HWND SettingsWindow::AddCheckbox(int id, int x, int y, int w, int h, const wchar_t* text) {
    HWND ctl = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                               x, y, w, h, content_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               instance_, nullptr);
    if (font_) SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return ctl;
}

HWND SettingsWindow::AddButton(HWND parent, int id, int x, int y, int w, int h, const wchar_t* text) {
    HWND ctl = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               instance_, nullptr);
    if (font_) SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return ctl;
}

HWND SettingsWindow::AddCombo(int id, int x, int y, int w, int h) {
    HWND ctl = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                               x, y, w, h, content_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               instance_, nullptr);
    if (font_) SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return ctl;
}

void SettingsWindow::BuildControls() {
    // Viewport clips scrolling content so it never paints over the footer.
    viewport_ = CreateWindowExW(
        0, kViewportClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, kContentW, 100,
        hwnd_, nullptr, instance_, nullptr);

    content_ = CreateWindowExW(
        0, kContentClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, kContentW, 100,
        viewport_, nullptr, instance_, nullptr);
    SetWindowLongPtrW(content_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    int y = 16;
    const int labelW = 150;
    const int trackX = 168;
    const int trackW = 300;

    auto row = [&](const wchar_t* label, int id, int minV, int maxV, int pos) {
        AddLabel(16, y + 4, labelW, 20, label);
        AddTrackbar(id, trackX, y, trackW, 28, minV, maxV, pos);
        y += 34;
    };

    AddLabel(16, y, 400, 22, L"Ambience");
    y += 28;

    row(L"Overlay Opacity", IDC_OPACITY, 5, 100, 55);
    row(L"Scene Opacity", IDC_SCENE_OPACITY, 0, 100, 0);
    row(L"Leaf Opacity", IDC_LEAF_OPACITY, 0, 100, 55);
    row(L"Leaf Count", IDC_LEAF_COUNT, 50, 4000, 700);
    row(L"Leaf Size", IDC_LEAF_SIZE, 6, 40, 15);

    AddLabel(16, y + 4, labelW, 20, L"Leaf Palette");
    HWND pal = AddCombo(IDC_PALETTE, trackX, y, 200, 200);
    SendMessageW(pal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Autumn"));
    SendMessageW(pal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sakura"));
    SendMessageW(pal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Amber"));
    SendMessageW(pal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cyber"));
    SendMessageW(pal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Midnight"));
    SendMessageW(pal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Mono"));
    SendMessageW(pal, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Crimson"));
    SendMessageW(pal, CB_SETCURSEL, 0, 0);
    y += 36;

    AddLabel(16, y, 400, 22, L"Physics");
    y += 28;
    row(L"Gravity", IDC_GRAVITY, 0, 200, 55);
    row(L"Drag", IDC_DRAG, 1, 80, 14);
    row(L"Base Wind", IDC_WIND, 0, 100, 18);
    row(L"Turbulence", IDC_TURBULENCE, 0, 100, 22);
    row(L"Cursor Radius", IDC_CURSOR_RADIUS, 20, 400, 160);
    row(L"Cursor Repulsion", IDC_CURSOR_REPULSION, 0, 100, 42);
    row(L"Cursor Vortex", IDC_CURSOR_VORTEX, 0, 100, 18);
    row(L"Cursor Wake", IDC_CURSOR_WAKE, 0, 100, 26);
    row(L"Force Smoothing", IDC_CURSOR_SMOOTH, 1, 30, 8);

    AddLabel(16, y, 400, 22, L"Display");
    y += 28;
    row(L"ASCII Cell Size", IDC_ASCII_SIZE, 8, 28, 14);
    row(L"CRT Strength", IDC_CRT, 0, 100, 10);
    row(L"Scanlines", IDC_SCANLINE, 0, 100, 6);

    AddLabel(16, y + 4, labelW, 20, L"FPS Limit");
    HWND fps = AddCombo(IDC_FPS, trackX, y, 160, 200);
    SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"30"));
    SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"60"));
    SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"120"));
    SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"144"));
    SendMessageW(fps, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Unlimited"));
    SendMessageW(fps, CB_SETCURSEL, 1, 0);
    y += 34;

    AddLabel(16, y + 4, labelW, 20, L"Monitor");
    HWND mon = AddCombo(IDC_MONITOR, trackX, y, 220, 200);
    SendMessageW(mon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All Monitors"));
    SendMessageW(mon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Monitor 1"));
    SendMessageW(mon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Monitor 2"));
    SendMessageW(mon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Monitor 3"));
    SendMessageW(mon, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Monitor 4"));
    SendMessageW(mon, CB_SETCURSEL, 1, 0);
    y += 36;

    AddCheckbox(IDC_ENABLED, 16, y, 200, 24, L"Overlay Enabled");
    y += 28;
    AddCheckbox(IDC_SHOW_SCENE, 16, y, 260, 24, L"Show ASCII Scene");
    y += 24;

    contentHeight_ = y + 16;

    // Buttons live on the main window (fixed footer), not in the scrolling content.
    btnApply_ = AddButton(hwnd_, IDC_APPLY, 16, 0, 100, 30, L"Apply");
    btnReset_ = AddButton(hwnd_, IDC_RESET, 128, 0, 130, 30, L"Reset Defaults");
    btnClose_ = AddButton(hwnd_, IDC_CLOSE, 270, 0, 100, 30, L"Close");
}

void SettingsWindow::Relayout() {
    if (!hwnd_ || !viewport_ || !content_) {
        return;
    }

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int clientW = static_cast<int>(rc.right - rc.left);
    const int clientH = static_cast<int>(rc.bottom - rc.top);
    const int viewH = (std::max)(0, clientH - kButtonBarH);
    const int contentW = (std::max)(kContentW, clientW);

    const int maxScroll = (std::max)(0, contentHeight_ - viewH);
    if (scrollPos_ > maxScroll) scrollPos_ = maxScroll;
    if (scrollPos_ < 0) scrollPos_ = 0;

    // Fixed viewport rectangle (clips overflowing content).
    SetWindowPos(viewport_, nullptr,
                 0, 0, clientW, viewH,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Move content inside viewport.
    SetWindowPos(content_, nullptr,
                 0, -scrollPos_,
                 contentW, contentHeight_,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    const int by = clientH - kButtonBarH + 12;
    SetWindowPos(btnApply_, HWND_TOP, 16, by, 100, 30, SWP_NOACTIVATE);
    SetWindowPos(btnReset_, HWND_TOP, 128, by, 130, 30, SWP_NOACTIVATE);
    SetWindowPos(btnClose_, HWND_TOP, 270, by, 100, 30, SWP_NOACTIVATE);

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = (std::max)(0, contentHeight_ - 1);
    si.nPage = static_cast<UINT>((std::max)(1, viewH));
    si.nPos = scrollPos_;
    SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);

    InvalidateRect(hwnd_, nullptr, TRUE);
    InvalidateRect(viewport_, nullptr, TRUE);
    InvalidateRect(content_, nullptr, TRUE);
}

void SettingsWindow::SetScrollPosClamped(int pos) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int viewH = (std::max)(0, static_cast<int>(rc.bottom - rc.top) - kButtonBarH);
    const int maxScroll = (std::max)(0, contentHeight_ - viewH);
    if (pos < 0) pos = 0;
    if (pos > maxScroll) pos = maxScroll;
    if (pos == scrollPos_) {
        return;
    }
    scrollPos_ = pos;
    Relayout();
}

void SettingsWindow::SyncFromConfig() {
    auto set = [&](int id, int value) {
        SetTrackPos(GetDlgItem(content_, id), value);
    };
    set(IDC_OPACITY, static_cast<int>(config_.overlayOpacity * 100));
    set(IDC_SCENE_OPACITY, static_cast<int>(config_.sceneOpacity * 100));
    set(IDC_LEAF_OPACITY, static_cast<int>(config_.leafOpacity * 100));
    set(IDC_LEAF_COUNT, config_.leafCount);
    set(IDC_LEAF_SIZE, static_cast<int>((config_.leafSizeMin + config_.leafSizeMax) * 0.5f));
    set(IDC_GRAVITY, static_cast<int>(config_.gravity));
    set(IDC_DRAG, static_cast<int>(config_.drag * 10));
    set(IDC_WIND, static_cast<int>(std::abs(config_.baseWind)));
    set(IDC_TURBULENCE, static_cast<int>(config_.turbulence));
    set(IDC_CURSOR_RADIUS, static_cast<int>(config_.cursorRadius));
    set(IDC_CURSOR_REPULSION, static_cast<int>(config_.cursorRepulsion / 10.0f));
    set(IDC_CURSOR_VORTEX, static_cast<int>(config_.cursorVortex / 10.0f));
    set(IDC_CURSOR_WAKE, static_cast<int>(config_.cursorWake / 10.0f));
    set(IDC_CURSOR_SMOOTH, static_cast<int>(config_.cursorForceSmoothing));
    set(IDC_ASCII_SIZE, config_.asciiCellSize);
    set(IDC_CRT, static_cast<int>(config_.crtStrength * 100));
    set(IDC_SCANLINE, static_cast<int>(config_.scanlineStrength * 100));

    SendMessageW(GetDlgItem(content_, IDC_PALETTE), CB_SETCURSEL, config_.leafPalette, 0);

    int fpsSel = 1;
    switch (config_.fpsLimit) {
    case 30: fpsSel = 0; break;
    case 60: fpsSel = 1; break;
    case 120: fpsSel = 2; break;
    case 144: fpsSel = 3; break;
    case 0: fpsSel = 4; break;
    default: fpsSel = 1; break;
    }
    SendMessageW(GetDlgItem(content_, IDC_FPS), CB_SETCURSEL, fpsSel, 0);
    SendMessageW(GetDlgItem(content_, IDC_MONITOR), CB_SETCURSEL,
                 config_.monitorIndex < 0 ? 0 : config_.monitorIndex + 1, 0);
    SendMessageW(GetDlgItem(content_, IDC_ENABLED), BM_SETCHECK,
                 config_.overlayEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(GetDlgItem(content_, IDC_SHOW_SCENE), BM_SETCHECK,
                 config_.showScene ? BST_CHECKED : BST_UNCHECKED, 0);
}

void SettingsWindow::SyncToConfig() {
    auto get = [&](int id) { return TrackPos(GetDlgItem(content_, id)); };
    config_.overlayOpacity = get(IDC_OPACITY) / 100.0f;
    config_.sceneOpacity = get(IDC_SCENE_OPACITY) / 100.0f;
    config_.leafOpacity = get(IDC_LEAF_OPACITY) / 100.0f;
    config_.leafCount = get(IDC_LEAF_COUNT);
    const float size = static_cast<float>(get(IDC_LEAF_SIZE));
    config_.leafSizeMin = size * 0.75f;
    config_.leafSizeMax = size * 1.25f;
    config_.gravity = static_cast<float>(get(IDC_GRAVITY));
    config_.drag = get(IDC_DRAG) / 10.0f;
    config_.baseWind = static_cast<float>(get(IDC_WIND));
    config_.turbulence = static_cast<float>(get(IDC_TURBULENCE));
    config_.cursorRadius = static_cast<float>(get(IDC_CURSOR_RADIUS));
    config_.cursorRepulsion = get(IDC_CURSOR_REPULSION) * 10.0f;
    config_.cursorVortex = get(IDC_CURSOR_VORTEX) * 10.0f;
    config_.cursorWake = get(IDC_CURSOR_WAKE) * 10.0f;
    config_.cursorForceSmoothing = static_cast<float>(get(IDC_CURSOR_SMOOTH));
    config_.asciiCellSize = get(IDC_ASCII_SIZE);
    config_.crtStrength = get(IDC_CRT) / 100.0f;
    config_.scanlineStrength = get(IDC_SCANLINE) / 100.0f;

    const int palSel = static_cast<int>(SendMessageW(GetDlgItem(content_, IDC_PALETTE), CB_GETCURSEL, 0, 0));
    if (palSel >= 0) {
        config_.leafPalette = palSel;
    }

    const int fpsSel = static_cast<int>(SendMessageW(GetDlgItem(content_, IDC_FPS), CB_GETCURSEL, 0, 0));
    const int fpsValues[] = {30, 60, 120, 144, 0};
    config_.fpsLimit = (fpsSel >= 0 && fpsSel < 5) ? fpsValues[fpsSel] : 60;

    const int monSel = static_cast<int>(SendMessageW(GetDlgItem(content_, IDC_MONITOR), CB_GETCURSEL, 0, 0));
    config_.monitorIndex = (monSel <= 0) ? -1 : (monSel - 1);

    config_.overlayEnabled = SendMessageW(GetDlgItem(content_, IDC_ENABLED), BM_GETCHECK, 0, 0) == BST_CHECKED;
    config_.showScene = SendMessageW(GetDlgItem(content_, IDC_SHOW_SCENE), BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (config_.showScene && config_.sceneOpacity < 0.05f) {
        config_.sceneOpacity = 0.35f;
    }
    if (!config_.showScene) {
        config_.sceneOpacity = 0.0f;
    }
    config_.Clamp();
}

void SettingsWindow::NotifyChanged() {
    SyncToConfig();
    if (onChanged_) {
        onChanged_(config_);
    }
}

LRESULT SettingsWindow::HandleContentMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        // Forward to parent so button/combo handlers live in one place.
        return SendMessageW(hwnd_, msg, wParam, lParam);
    }
    case WM_HSCROLL:
        NotifyChanged();
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, kText);
        SetBkColor(hdc, kBg);
        return reinterpret_cast<LRESULT>(bgBrush_);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, kText);
        SetBkColor(hdc, kEditBg);
        return reinterpret_cast<LRESULT>(editBrush_);
    }
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc{};
        GetClientRect(content_, &rc);
        FillRect(hdc, &rc, bgBrush_);
        return 1;
    }
    case WM_MOUSEWHEEL: {
        // Bubble wheel to parent scroller.
        return SendMessageW(hwnd_, msg, wParam, lParam);
    }
    default:
        break;
    }
    return DefWindowProcW(content_, msg, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        Relayout();
        return 0;

    case WM_VSCROLL: {
        SCROLLINFO si{sizeof(si), SIF_ALL};
        GetScrollInfo(hwnd_, SB_VERT, &si);
        int pos = scrollPos_;
        switch (LOWORD(wParam)) {
        case SB_LINEUP: pos -= 32; break;
        case SB_LINEDOWN: pos += 32; break;
        case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
        case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
        case SB_TOP: pos = 0; break;
        case SB_BOTTOM: pos = si.nMax; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        default: break;
        }
        SetScrollPosClamped(pos);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        const int delta = -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
        SetScrollPosClamped(scrollPos_ + delta);
        return 0;
    }

    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        const int viewH = (std::max)(0, static_cast<int>(rc.bottom - rc.top) - kButtonBarH);

        RECT top = rc;
        top.bottom = viewH;
        FillRect(hdc, &top, bgBrush_);

        RECT bar = rc;
        bar.top = viewH;
        FillRect(hdc, &bar, panelBrush_);
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, kText);
        SetBkColor(hdc, kPanel);
        return reinterpret_cast<LRESULT>(panelBrush_);
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, kText);
        SetBkColor(hdc, kEditBg);
        return reinterpret_cast<LRESULT>(editBrush_);
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == IDC_APPLY || id == IDC_ENABLED || id == IDC_SHOW_SCENE ||
            (code == CBN_SELCHANGE && (id == IDC_FPS || id == IDC_MONITOR || id == IDC_PALETTE))) {
            NotifyChanged();
            return 0;
        }
        if (id == IDC_RESET) {
            if (onReset_) onReset_();
            config_.ResetToDefaults();
            SyncFromConfig();
            NotifyChanged();
            return 0;
        }
        if (id == IDC_CLOSE) {
            Hide();
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        Hide();
        return 0;

    case WM_DESTROY:
        hwnd_ = nullptr;
        viewport_ = nullptr;
        content_ = nullptr;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

} // namespace rjo
