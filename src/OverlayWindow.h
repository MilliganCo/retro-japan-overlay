#pragma once

#include <Windows.h>
#include <functional>

namespace rjo {

class OverlayWindow {
public:
    using DisplayChangeCallback = std::function<void()>;

    bool Create(HINSTANCE instance);
    void Destroy();

    HWND Handle() const { return hwnd_; }
    void SetBounds(const RECT& rect);
    RECT Bounds() const { return bounds_; }
    void SetVisible(bool visible);
    bool IsVisible() const { return visible_; }
    void EnsureClickThrough();

    void SetDisplayChangeCallback(DisplayChangeCallback cb) { onDisplayChange_ = std::move(cb); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static DWORD DesiredExStyle();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    RECT bounds_{};
    bool visible_ = false;
    DisplayChangeCallback onDisplayChange_;
};

} // namespace rjo
