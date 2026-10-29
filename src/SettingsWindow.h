#pragma once

#include "Configuration.h"

#include <Windows.h>
#include <functional>
#include <vector>

namespace rjo {

class SettingsWindow {
public:
    using ConfigChangedHandler = std::function<void(const AppConfig&)>;
    using ResetHandler = std::function<void()>;

    bool Create(HINSTANCE instance, HWND parentMessageHwnd);
    void Destroy();
    void Show(const AppConfig& config);
    void Hide();
    bool IsVisible() const;
    HWND Handle() const { return hwnd_; }

    void SetConfigChangedHandler(ConfigChangedHandler handler) { onChanged_ = std::move(handler); }
    void SetResetHandler(ResetHandler handler) { onReset_ = std::move(handler); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ContentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleContentMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void BuildControls();
    void Relayout();
    void SyncFromConfig();
    void SyncToConfig();
    void NotifyChanged();
    void SetScrollPosClamped(int pos);
    HWND AddLabel(int x, int y, int w, int h, const wchar_t* text);
    HWND AddTrackbar(int id, int x, int y, int w, int h, int minV, int maxV, int pos);
    HWND AddCheckbox(int id, int x, int y, int w, int h, const wchar_t* text);
    HWND AddButton(HWND parent, int id, int x, int y, int w, int h, const wchar_t* text);
    HWND AddCombo(int id, int x, int y, int w, int h);

    HINSTANCE instance_ = nullptr;
    HWND parent_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND viewport_ = nullptr;
    HWND content_ = nullptr;
    HWND btnApply_ = nullptr;
    HWND btnReset_ = nullptr;
    HWND btnClose_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HFONT font_ = nullptr;
    AppConfig config_{};
    ConfigChangedHandler onChanged_;
    ResetHandler onReset_;
    int contentHeight_ = 0;
    int scrollPos_ = 0;
    static constexpr int kButtonBarH = 56;
    static constexpr int kContentW = 500;
};

} // namespace rjo
