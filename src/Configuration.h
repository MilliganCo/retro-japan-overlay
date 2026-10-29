#pragma once

#include <string>
#include <cstdint>

namespace rjo {

enum class LeafPalette : int {
    Autumn = 0,
    Sakura = 1,
    Amber = 2,
    Cyber = 3,
    Midnight = 4,
    Mono = 5,
    Crimson = 6,
    Count
};

struct AppConfig {
    int version = 3;

    // Overlay
    bool overlayEnabled = true;
    bool paused = false;
    bool showScene = false; // ASCII sunset backdrop (off by default — leaves only)
    float overlayOpacity = 0.55f;
    float sceneOpacity = 0.0f;
    float leafOpacity = 0.55f;
    float sceneBrightness = 1.0f;
    int fpsLimit = 60; // 0 = unlimited
    int monitorIndex = 0; // default: primary monitor (faster + reliable layered present)
    bool startWithWindows = false;
    int asciiCellSize = 14;
    float crtStrength = 0.10f;
    float scanlineStrength = 0.06f;
    float ditherStrength = 0.04f;
    float grainStrength = 0.02f;
    float glowStrength = 0.12f;

    // Leaves
    int leafCount = 700;
    float leafSizeMin = 10.0f;
    float leafSizeMax = 20.0f;
    float gravity = 55.0f;
    float drag = 1.35f;
    float baseWind = 18.0f;
    float windStrength = 1.0f;
    float turbulence = 22.0f;
    float animationSpeed = 1.0f;
    float leafReactionStrength = 1.0f;
    int leafPalette = static_cast<int>(LeafPalette::Autumn);

    // Cursor interaction
    float cursorRadius = 160.0f;
    float cursorRepulsion = 420.0f;
    float cursorVortex = 180.0f;
    float cursorWake = 260.0f;
    float cursorForceSmoothing = 8.0f;
    float cursorGustScale = 1.0f;
    float maxLeafSpeed = 520.0f;

    void Clamp();
    void ResetToDefaults();
};

class Configuration {
public:
    bool Load(const std::wstring& path);
    bool Save(const std::wstring& path) const;
    bool LoadOrCreateDefaults(const std::wstring& path);

    AppConfig& Get() { return config_; }
    const AppConfig& Get() const { return config_; }
    void Set(const AppConfig& cfg) { config_ = cfg; config_.Clamp(); }

private:
    AppConfig config_{};
};

bool SetStartWithWindows(bool enabled);
bool IsStartWithWindowsEnabled();

} // namespace rjo
