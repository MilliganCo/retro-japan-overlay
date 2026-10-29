#include "OverlayWindow.h"
#include "Utilities/Logger.h"

namespace rjo {

namespace {
constexpr wchar_t kOverlayClass[] = L"RetroJapanOverlay.OverlayWindow";
}

DWORD OverlayWindow::DesiredExStyle() {
    // Classic layered window: UpdateLayeredWindow + WS_EX_TRANSPARENT =
    // reliable mouse/hover pass-through (no WS_EX_NOREDIRECTIONBITMAP).
    return WS_EX_LAYERED |
           WS_EX_TRANSPARENT |
           WS_EX_TOPMOST |
           WS_EX_TOOLWINDOW |
           WS_EX_NOACTIVATE;
}

bool OverlayWindow::Create(HINSTANCE instance) {
    instance_ = instance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWindow::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kOverlayClass;
    wc.hCursor = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::Instance().Error("Failed to register overlay window class");
        return false;
    }

    hwnd_ = CreateWindowExW(
        DesiredExStyle(),
        kOverlayClass,
        L"RetroJapanOverlay",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr,
        nullptr,
        instance,
        this);

    if (!hwnd_) {
        Logger::Instance().Error("Failed to create overlay window");
        return false;
    }

    EnsureClickThrough();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;

    Logger::Instance().Info("Overlay window created (layered UpdateLayeredWindow click-through)");
    return true;
}

void OverlayWindow::EnsureClickThrough() {
    if (!hwnd_) {
        return;
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, DesiredExStyle());
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void OverlayWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void OverlayWindow::SetBounds(const RECT& rect) {
    bounds_ = rect;
    if (!hwnd_) {
        return;
    }
    // Position/size for UpdateLayeredWindow destination; still keep HWND in sync.
    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_FRAMECHANGED);
    EnsureClickThrough();
}

void OverlayWindow::SetVisible(bool visible) {
    visible_ = visible;
    if (!hwnd_) {
        return;
    }
    ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (visible) {
        EnsureClickThrough();
    }
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ACTIVATE:
    case WM_SETFOCUS:
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
    case WM_SETTINGCHANGE:
        if (onDisplayChange_) {
            onDisplayChange_();
        }
        return 0;
    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

} // namespace rjo
