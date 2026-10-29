#pragma once

#include "Configuration.h"
#include "D3DRenderer.h"
#include "MonitorManager.h"
#include "MouseField.h"
#include "OverlayWindow.h"
#include "ParticleSystem.h"
#include "SceneRenderer.h"
#include "SettingsWindow.h"
#include "TrayController.h"
#include "Utilities/Timer.h"
#include "Utilities/OlkToggle.h"

#include <Windows.h>
#include <atomic>
#include <vector>

namespace rjo {

class Application {
public:
    bool Initialize(HINSTANCE instance);
    int Run();
    void RequestExit();

private:
    bool CreateMessageWindow();
    void ApplyDisplayTopology();
    void RebuildScene();
    void ApplyConfig(const AppConfig& cfg, bool save);
    void HandleTrayCommand(TrayCommand cmd, int arg);
    void OpenSettings();
    void Frame();
    void PumpMessages();
    std::vector<std::wstring> MonitorNames() const;

    static LRESULT CALLBACK MessageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_ = nullptr;
    HWND messageHwnd_ = nullptr;
    std::atomic<bool> running_{true};
    bool needsSceneRebuild_ = true;
    bool needsResize_ = false;
    int lastAsciiCell_ = -1;
    int lastLeafCount_ = -1;
    int lastLeafPalette_ = -1;

    Configuration configuration_;
    MonitorManager monitors_;
    OverlayWindow overlay_;
    D3DRenderer renderer_;
    SceneRenderer scene_;
    ParticleSystem particles_;
    MouseField mouse_;
    TrayController tray_;
    SettingsWindow settings_;
    OlkToggle olkToggle_;

    HighResolutionTimer timer_;
    HighResolutionTimer frameLimiter_;
    float timeSeconds_ = 0.0f;
    std::vector<LeafInstanceGpu> leafInstances_;
    RECT overlayRect_{};
};

} // namespace rjo
