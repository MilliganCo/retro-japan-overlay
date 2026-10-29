#include "Application.h"
#include "Utilities/Logger.h"
#include "Utilities/Paths.h"

#include <ShellScalingApi.h>
#include <algorithm>
#include <string>

#pragma comment(lib, "Shcore.lib")

namespace rjo {

namespace {
constexpr wchar_t kMessageClass[] = L"RetroJapanOverlay.MessageWindow";
constexpr UINT WM_APP_WAKE = WM_APP + 77;
}

bool Application::Initialize(HINSTANCE instance) {
    instance_ = instance;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    EnsureAppDirectories();
    Logger::Instance().Initialize(GetLogDirectory());
    Logger::Instance().Info("RetroJapanOverlay starting");

    if (!configuration_.LoadOrCreateDefaults(GetConfigPath())) {
        Logger::Instance().Warning("Using in-memory defaults");
    }

    AppConfig& cfg = configuration_.Get();
    cfg.startWithWindows = IsStartWithWindowsEnabled();
    cfg.Clamp();

    if (!CreateMessageWindow()) {
        Logger::Instance().Error("Failed to create message window");
        return false;
    }
    Logger::Instance().Info("Message window ready");

    monitors_.Refresh();

    if (!overlay_.Create(instance_)) {
        return false;
    }
    overlay_.SetDisplayChangeCallback([this]() {
        PostMessageW(messageHwnd_, WM_DISPLAYCHANGE, 0, 0);
    });

    ApplyDisplayTopology();

    const UINT width = static_cast<UINT>(overlayRect_.right - overlayRect_.left);
    const UINT height = static_cast<UINT>(overlayRect_.bottom - overlayRect_.top);

    if (!renderer_.Initialize(overlay_.Handle(), width, height)) {
        MessageBoxW(nullptr, L"Failed to initialize Direct3D 11 renderer.", L"RetroJapanOverlay",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    overlay_.EnsureClickThrough();
    Logger::Instance().Info("Using UpdateLayeredWindow present path (true click-through)");

    particles_.Initialize(cfg.leafCount);
    particles_.ResetAll(static_cast<float>(width), static_cast<float>(height), cfg);
    lastLeafCount_ = cfg.leafCount;
    lastLeafPalette_ = cfg.leafPalette;
    lastAsciiCell_ = cfg.asciiCellSize;
    needsSceneRebuild_ = true;

    if (!tray_.Create(instance_, messageHwnd_)) {
        return false;
    }
    tray_.SetCommandHandler([this](TrayCommand cmd, int arg) { HandleTrayCommand(cmd, arg); });
    tray_.SetSettingsHandler([this]() { OpenSettings(); });

    if (!settings_.Create(instance_, messageHwnd_)) {
        return false;
    }
    settings_.SetConfigChangedHandler([this](const AppConfig& c) { ApplyConfig(c, true); });
    settings_.SetResetHandler([this]() {
        AppConfig defaults;
        defaults.startWithWindows = configuration_.Get().startWithWindows;
        ApplyConfig(defaults, true);
    });

    overlay_.SetVisible(cfg.overlayEnabled);
    tray_.UpdateMenuContext(cfg, MonitorNames());
    timer_.Reset();
    frameLimiter_.Reset();
    Logger::Instance().Info("Initialization complete");
    return true;
}

bool Application::CreateMessageWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = Application::MessageWndProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kMessageClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::Instance().Error("Failed to register message window class");
        return false;
    }

    messageHwnd_ = CreateWindowExW(
        0, kMessageClass, L"RetroJapanOverlay.Message",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance_, this);
    if (!messageHwnd_) {
        char buf[128]{};
        sprintf_s(buf, "CreateWindowEx message window failed (err=%lu)", GetLastError());
        Logger::Instance().Error(buf);
        return false;
    }
    return true;
}

LRESULT CALLBACK Application::MessageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Application* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Application*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        // Must assign before any DefWindowProc during CreateWindowEx.
        self->messageHwnd_ = hwnd;
    } else {
        self = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Application::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (tray_.HandleMessage(msg, wParam, lParam)) {
        return 0;
    }

    switch (msg) {
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
    case WM_SETTINGCHANGE:
        Logger::Instance().Info("Display topology/settings changed");
        monitors_.Refresh();
        ApplyDisplayTopology();
        needsResize_ = true;
        needsSceneRebuild_ = true;
        return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            Logger::Instance().Info("System resumed from sleep");
            needsResize_ = true;
            needsSceneRebuild_ = true;
        }
        return TRUE;
    case WM_APP_WAKE:
        // Second instance asked us to surface tray UX.
        tray_.UpdateTip(L"RetroJapanOverlay is already running");
        return 0;
    case WM_DESTROY:
        running_ = false;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Application::ApplyDisplayTopology() {
    const AppConfig& cfg = configuration_.Get();
    overlayRect_ = monitors_.ResolveTargetRect(cfg.monitorIndex);
    overlay_.SetBounds(overlayRect_);
}

void Application::RebuildScene() {
    const AppConfig& cfg = configuration_.Get();
    const float w = static_cast<float>(renderer_.Width());
    const float h = static_cast<float>(renderer_.Height());
    scene_.Rebuild(w, h, cfg.asciiCellSize, renderer_.Atlas(), cfg);
    needsSceneRebuild_ = false;
    lastAsciiCell_ = cfg.asciiCellSize;
}

std::vector<std::wstring> Application::MonitorNames() const {
    std::vector<std::wstring> names;
    for (const auto& m : monitors_.Monitors()) {
        names.push_back(m.name);
    }
    return names;
}

void Application::ApplyConfig(const AppConfig& cfgIn, bool save) {
    AppConfig cfg = cfgIn;
    cfg.Clamp();
    configuration_.Set(cfg);

    if (cfg.startWithWindows != IsStartWithWindowsEnabled()) {
        SetStartWithWindows(cfg.startWithWindows);
    }

    ApplyDisplayTopology();
    needsResize_ = true;

    if (cfg.leafCount != lastLeafCount_) {
        particles_.Resize(cfg.leafCount);
        particles_.ResetAll(static_cast<float>(renderer_.Width()), static_cast<float>(renderer_.Height()), cfg);
        lastLeafCount_ = cfg.leafCount;
        lastLeafPalette_ = cfg.leafPalette;
    } else if (cfg.leafPalette != lastLeafPalette_) {
        particles_.RecolorAll(cfg);
        lastLeafPalette_ = cfg.leafPalette;
        Logger::Instance().Info("Leaf palette updated");
    }

    needsSceneRebuild_ = true;

    // Keep window present while paused (frozen frame); hide only when disabled.
    overlay_.SetVisible(cfg.overlayEnabled);
    if (cfg.overlayEnabled) {
        overlay_.EnsureClickThrough();
    }

    if (save) {
        configuration_.Save(GetConfigPath());
    }

    tray_.UpdateMenuContext(cfg, MonitorNames());
}

void Application::HandleTrayCommand(TrayCommand cmd, int arg) {
    AppConfig cfg = configuration_.Get();

    switch (cmd) {
    case TrayCommand::PauseResume:
        cfg.paused = !cfg.paused;
        break;
    case TrayCommand::EnableDisable:
        cfg.overlayEnabled = !cfg.overlayEnabled;
        break;
    case TrayCommand::OpenSettings:
        OpenSettings();
        return;
    case TrayCommand::ReloadConfig:
        configuration_.Load(GetConfigPath());
        ApplyConfig(configuration_.Get(), false);
        return;
    case TrayCommand::Exit:
        RequestExit();
        return;
    case TrayCommand::StartWithWindows:
        cfg.startWithWindows = !cfg.startWithWindows;
        break;
    case TrayCommand::Fps30: cfg.fpsLimit = 30; break;
    case TrayCommand::Fps60: cfg.fpsLimit = 60; break;
    case TrayCommand::Fps120: cfg.fpsLimit = 120; break;
    case TrayCommand::Fps144: cfg.fpsLimit = 144; break;
    case TrayCommand::FpsUnlimited: cfg.fpsLimit = 0; break;
    case TrayCommand::DensityLow: cfg.leafCount = 300; break;
    case TrayCommand::DensityMed: cfg.leafCount = 900; break;
    case TrayCommand::DensityHigh: cfg.leafCount = 1600; break;
    case TrayCommand::DensityUltra: cfg.leafCount = 3000; break;
    case TrayCommand::ReactionLow: cfg.leafReactionStrength = 0.45f; break;
    case TrayCommand::ReactionMed: cfg.leafReactionStrength = 1.0f; break;
    case TrayCommand::ReactionHigh: cfg.leafReactionStrength = 1.8f; break;
    case TrayCommand::WindLow: cfg.windStrength = 0.35f; cfg.baseWind = 8.0f; break;
    case TrayCommand::WindMed: cfg.windStrength = 1.0f; cfg.baseWind = 18.0f; break;
    case TrayCommand::WindHigh: cfg.windStrength = 1.8f; cfg.baseWind = 34.0f; break;
    case TrayCommand::SpeedSlow: cfg.animationSpeed = 0.55f; break;
    case TrayCommand::SpeedNormal: cfg.animationSpeed = 1.0f; break;
    case TrayCommand::SpeedFast: cfg.animationSpeed = 1.55f; break;
    case TrayCommand::BrightLow: cfg.sceneBrightness = 0.65f; break;
    case TrayCommand::BrightMed: cfg.sceneBrightness = 1.0f; break;
    case TrayCommand::BrightHigh: cfg.sceneBrightness = 1.35f; break;
    case TrayCommand::OpacityLow: cfg.overlayOpacity = 0.35f; cfg.leafOpacity = 0.40f; break;
    case TrayCommand::OpacityMed: cfg.overlayOpacity = 0.55f; cfg.leafOpacity = 0.55f; break;
    case TrayCommand::OpacityHigh: cfg.overlayOpacity = 0.75f; cfg.leafOpacity = 0.70f; break;
    case TrayCommand::PaletteAutumn: cfg.leafPalette = static_cast<int>(LeafPalette::Autumn); break;
    case TrayCommand::PaletteSakura: cfg.leafPalette = static_cast<int>(LeafPalette::Sakura); break;
    case TrayCommand::PaletteAmber: cfg.leafPalette = static_cast<int>(LeafPalette::Amber); break;
    case TrayCommand::PaletteCyber: cfg.leafPalette = static_cast<int>(LeafPalette::Cyber); break;
    case TrayCommand::PaletteMidnight: cfg.leafPalette = static_cast<int>(LeafPalette::Midnight); break;
    case TrayCommand::PaletteMono: cfg.leafPalette = static_cast<int>(LeafPalette::Mono); break;
    case TrayCommand::PaletteCrimson: cfg.leafPalette = static_cast<int>(LeafPalette::Crimson); break;
    case TrayCommand::ToggleScene:
        cfg.showScene = !cfg.showScene;
        cfg.sceneOpacity = cfg.showScene ? 0.35f : 0.0f;
        break;
    case TrayCommand::MonitorAll:
        cfg.monitorIndex = -1;
        break;
    case TrayCommand::MonitorBase:
        cfg.monitorIndex = arg;
        break;
    default:
        break;
    }

    ApplyConfig(cfg, true);
}

void Application::OpenSettings() {
    settings_.Show(configuration_.Get());
}

void Application::RequestExit() {
    running_ = false;
    PostMessageW(messageHwnd_, WM_CLOSE, 0, 0);
}

void Application::PumpMessages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            running_ = false;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Application::Frame() {
    double dt = timer_.Tick();
    if (dt > 0.05) {
        dt = 0.05;
    }
    const float fdt = static_cast<float>(dt);
    timeSeconds_ += fdt;

    AppConfig& cfg = configuration_.Get();

    if (needsResize_) {
        ApplyDisplayTopology();
        const UINT w = static_cast<UINT>((std::max)(1L, overlayRect_.right - overlayRect_.left));
        const UINT h = static_cast<UINT>((std::max)(1L, overlayRect_.bottom - overlayRect_.top));
        renderer_.Resize(w, h);
        particles_.ResetAll(static_cast<float>(w), static_cast<float>(h), cfg);
        needsResize_ = false;
        needsSceneRebuild_ = true;
        overlay_.EnsureClickThrough();
    }

    // O → L → K toggles overlay visibility (polled, no hooks / no focus steal).
    if (olkToggle_.Update(fdt)) {
        cfg.overlayEnabled = !cfg.overlayEnabled;
        ApplyConfig(cfg, true);
        tray_.UpdateTip(cfg.overlayEnabled ? L"RetroJapanOverlay (visible)"
                                           : L"RetroJapanOverlay (hidden — press O,L,K)");
    }

    if (!cfg.overlayEnabled) {
        // Idle: keep message pump alive with low CPU.
        Sleep(50);
        return;
    }

    // Periodically reinforce click-through after DWM/shell events.
    static float clickThroughTimer = 0.0f;
    clickThroughTimer += fdt;
    if (clickThroughTimer > 2.0f) {
        overlay_.EnsureClickThrough();
        clickThroughTimer = 0.0f;
    }

    if (needsSceneRebuild_) {
        RebuildScene();
    }

    mouse_.Update(fdt, cfg, overlayRect_);

    if (!cfg.paused) {
        particles_.Update(fdt,
                          static_cast<float>(renderer_.Width()),
                          static_cast<float>(renderer_.Height()),
                          cfg,
                          mouse_);
    }

    particles_.BuildInstanceBuffer(leafInstances_, cfg.leafOpacity * cfg.overlayOpacity);
    renderer_.Render(scene_, leafInstances_, cfg, timeSeconds_);

    // CPU frame limiter for high refresh targets / unlimited with vsync off path.
    if (cfg.fpsLimit > 0) {
        const double target = 1.0 / static_cast<double>(cfg.fpsLimit);
        frameLimiter_.WaitUntil(target);
        frameLimiter_.Reset();
    } else {
        frameLimiter_.Reset();
    }
}

int Application::Run() {
    while (running_) {
        PumpMessages();
        if (!running_) {
            break;
        }
        Frame();
    }

    Logger::Instance().Info("Shutting down");
    configuration_.Save(GetConfigPath());
    settings_.Destroy();
    tray_.Destroy();
    renderer_.Shutdown();
    overlay_.Destroy();
    if (messageHwnd_) {
        DestroyWindow(messageHwnd_);
        messageHwnd_ = nullptr;
    }
    Logger::Instance().Info("Shutdown complete");
    return 0;
}

} // namespace rjo
