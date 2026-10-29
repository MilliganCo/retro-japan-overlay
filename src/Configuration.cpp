#include "Configuration.h"
#include "Utilities/Logger.h"
#include "Utilities/Math.h"
#include "Utilities/Paths.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#pragma comment(lib, "Shlwapi.lib")

namespace fs = std::filesystem;

namespace rjo {

namespace {

float ReadFloat(const std::wstring& path, const wchar_t* section, const wchar_t* key, float def) {
    wchar_t buf[64]{};
    GetPrivateProfileStringW(section, key, nullptr, buf, 64, path.c_str());
    if (buf[0] == L'\0') {
        return def;
    }
    return static_cast<float>(_wtof(buf));
}

int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int def) {
    return static_cast<int>(GetPrivateProfileIntW(section, key, def, path.c_str()));
}

bool ReadBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool def) {
    return ReadInt(path, section, key, def ? 1 : 0) != 0;
}

void WriteFloat(const std::wstring& path, const wchar_t* section, const wchar_t* key, float value) {
    wchar_t buf[64]{};
    swprintf_s(buf, L"%.4f", value);
    WritePrivateProfileStringW(section, key, buf, path.c_str());
}

void WriteInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int value) {
    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, path.c_str());
}

void WriteBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool value) {
    WriteInt(path, section, key, value ? 1 : 0);
}

constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kAppName = L"RetroJapanOverlay";

} // namespace

void AppConfig::Clamp() {
    overlayOpacity = rjo::Clamp(overlayOpacity, 0.05f, 1.0f);
    sceneOpacity = rjo::Clamp(sceneOpacity, 0.0f, 1.0f);
    leafOpacity = rjo::Clamp(leafOpacity, 0.0f, 1.0f);
    sceneBrightness = rjo::Clamp(sceneBrightness, 0.2f, 2.0f);
    if (fpsLimit < 0) fpsLimit = 0;
    if (fpsLimit > 0 && fpsLimit < 15) fpsLimit = 15;
    asciiCellSize = static_cast<int>(rjo::Clamp(static_cast<float>(asciiCellSize), 8.0f, 32.0f));
    crtStrength = rjo::Clamp(crtStrength, 0.0f, 1.0f);
    scanlineStrength = rjo::Clamp(scanlineStrength, 0.0f, 1.0f);
    ditherStrength = rjo::Clamp(ditherStrength, 0.0f, 1.0f);
    grainStrength = rjo::Clamp(grainStrength, 0.0f, 1.0f);
    glowStrength = rjo::Clamp(glowStrength, 0.0f, 1.0f);

    leafCount = static_cast<int>(rjo::Clamp(static_cast<float>(leafCount), 50.0f, 8000.0f));
    leafSizeMin = rjo::Clamp(leafSizeMin, 4.0f, 64.0f);
    leafSizeMax = rjo::Clamp(leafSizeMax, leafSizeMin, 96.0f);
    gravity = rjo::Clamp(gravity, 0.0f, 300.0f);
    drag = rjo::Clamp(drag, 0.1f, 8.0f);
    baseWind = rjo::Clamp(baseWind, -200.0f, 200.0f);
    windStrength = rjo::Clamp(windStrength, 0.0f, 3.0f);
    turbulence = rjo::Clamp(turbulence, 0.0f, 200.0f);
    animationSpeed = rjo::Clamp(animationSpeed, 0.1f, 3.0f);
    leafReactionStrength = rjo::Clamp(leafReactionStrength, 0.0f, 3.0f);
    if (leafPalette < 0) leafPalette = 0;
    if (leafPalette >= static_cast<int>(LeafPalette::Count)) {
        leafPalette = static_cast<int>(LeafPalette::Autumn);
    }

    cursorRadius = rjo::Clamp(cursorRadius, 20.0f, 600.0f);
    cursorRepulsion = rjo::Clamp(cursorRepulsion, 0.0f, 2000.0f);
    cursorVortex = rjo::Clamp(cursorVortex, 0.0f, 1000.0f);
    cursorWake = rjo::Clamp(cursorWake, 0.0f, 1500.0f);
    cursorForceSmoothing = rjo::Clamp(cursorForceSmoothing, 0.5f, 30.0f);
    cursorGustScale = rjo::Clamp(cursorGustScale, 0.0f, 3.0f);
    maxLeafSpeed = rjo::Clamp(maxLeafSpeed, 50.0f, 2000.0f);
}

void AppConfig::ResetToDefaults() {
    *this = AppConfig{};
}

bool Configuration::Load(const std::wstring& path) {
    if (!PathFileExistsW(path.c_str())) {
        Logger::Instance().Warning("Configuration file not found");
        return false;
    }

    AppConfig cfg;
    cfg.version = ReadInt(path, L"General", L"Version", 1);
    cfg.overlayEnabled = ReadBool(path, L"Overlay", L"Enabled", cfg.overlayEnabled);
    cfg.paused = ReadBool(path, L"Overlay", L"Paused", cfg.paused);
    cfg.showScene = ReadBool(path, L"Overlay", L"ShowScene", cfg.showScene);
    cfg.overlayOpacity = ReadFloat(path, L"Overlay", L"Opacity", cfg.overlayOpacity);
    cfg.sceneOpacity = ReadFloat(path, L"Overlay", L"SceneOpacity", cfg.sceneOpacity);
    cfg.leafOpacity = ReadFloat(path, L"Overlay", L"LeafOpacity", cfg.leafOpacity);
    cfg.sceneBrightness = ReadFloat(path, L"Overlay", L"Brightness", cfg.sceneBrightness);
    cfg.fpsLimit = ReadInt(path, L"Overlay", L"FpsLimit", cfg.fpsLimit);
    cfg.monitorIndex = ReadInt(path, L"Overlay", L"MonitorIndex", cfg.monitorIndex);
    cfg.startWithWindows = ReadBool(path, L"Overlay", L"StartWithWindows", cfg.startWithWindows);
    cfg.asciiCellSize = ReadInt(path, L"Overlay", L"AsciiCellSize", cfg.asciiCellSize);
    cfg.crtStrength = ReadFloat(path, L"Effects", L"CrtStrength", cfg.crtStrength);
    cfg.scanlineStrength = ReadFloat(path, L"Effects", L"ScanlineStrength", cfg.scanlineStrength);
    cfg.ditherStrength = ReadFloat(path, L"Effects", L"DitherStrength", cfg.ditherStrength);
    cfg.grainStrength = ReadFloat(path, L"Effects", L"GrainStrength", cfg.grainStrength);
    cfg.glowStrength = ReadFloat(path, L"Effects", L"GlowStrength", cfg.glowStrength);

    cfg.leafCount = ReadInt(path, L"Leaves", L"Count", cfg.leafCount);
    cfg.leafSizeMin = ReadFloat(path, L"Leaves", L"SizeMin", cfg.leafSizeMin);
    cfg.leafSizeMax = ReadFloat(path, L"Leaves", L"SizeMax", cfg.leafSizeMax);
    cfg.gravity = ReadFloat(path, L"Leaves", L"Gravity", cfg.gravity);
    cfg.drag = ReadFloat(path, L"Leaves", L"Drag", cfg.drag);
    cfg.baseWind = ReadFloat(path, L"Leaves", L"BaseWind", cfg.baseWind);
    cfg.windStrength = ReadFloat(path, L"Leaves", L"WindStrength", cfg.windStrength);
    cfg.turbulence = ReadFloat(path, L"Leaves", L"Turbulence", cfg.turbulence);
    cfg.animationSpeed = ReadFloat(path, L"Leaves", L"AnimationSpeed", cfg.animationSpeed);
    cfg.leafReactionStrength = ReadFloat(path, L"Leaves", L"ReactionStrength", cfg.leafReactionStrength);
    cfg.leafPalette = ReadInt(path, L"Leaves", L"Palette", cfg.leafPalette);

    cfg.cursorRadius = ReadFloat(path, L"Cursor", L"Radius", cfg.cursorRadius);
    cfg.cursorRepulsion = ReadFloat(path, L"Cursor", L"Repulsion", cfg.cursorRepulsion);
    cfg.cursorVortex = ReadFloat(path, L"Cursor", L"Vortex", cfg.cursorVortex);
    cfg.cursorWake = ReadFloat(path, L"Cursor", L"Wake", cfg.cursorWake);
    cfg.cursorForceSmoothing = ReadFloat(path, L"Cursor", L"ForceSmoothing", cfg.cursorForceSmoothing);
    cfg.cursorGustScale = ReadFloat(path, L"Cursor", L"GustScale", cfg.cursorGustScale);
    cfg.maxLeafSpeed = ReadFloat(path, L"Cursor", L"MaxLeafSpeed", cfg.maxLeafSpeed);

    // Migrate v1 → ambience defaults (leaves-only, lighter opacity).
    if (cfg.version < 2) {
        cfg.showScene = false;
        cfg.sceneOpacity = 0.0f;
        cfg.overlayOpacity = (std::min)(cfg.overlayOpacity, 0.55f);
        cfg.leafOpacity = (std::min)(cfg.leafOpacity, 0.55f);
        cfg.glowStrength = (std::min)(cfg.glowStrength, 0.12f);
        cfg.monitorIndex = 0;
        cfg.version = 2;
    }
    // v2 → v3: force primary-monitor default when still on "all monitors"
    // after switching to UpdateLayeredWindow (full virtual desktop is heavy).
    if (cfg.version < 3) {
        if (cfg.monitorIndex < 0) {
            cfg.monitorIndex = 0;
        }
        cfg.version = 3;
    }

    cfg.Clamp();
    config_ = cfg;
    Logger::Instance().Info("Configuration loaded");
    return true;
}

bool Configuration::Save(const std::wstring& path) const {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);

    AppConfig cfg = config_;
    cfg.Clamp();

    WriteInt(path, L"General", L"Version", cfg.version);
    WriteBool(path, L"Overlay", L"Enabled", cfg.overlayEnabled);
    WriteBool(path, L"Overlay", L"Paused", cfg.paused);
    WriteBool(path, L"Overlay", L"ShowScene", cfg.showScene);
    WriteFloat(path, L"Overlay", L"Opacity", cfg.overlayOpacity);
    WriteFloat(path, L"Overlay", L"SceneOpacity", cfg.sceneOpacity);
    WriteFloat(path, L"Overlay", L"LeafOpacity", cfg.leafOpacity);
    WriteFloat(path, L"Overlay", L"Brightness", cfg.sceneBrightness);
    WriteInt(path, L"Overlay", L"FpsLimit", cfg.fpsLimit);
    WriteInt(path, L"Overlay", L"MonitorIndex", cfg.monitorIndex);
    WriteBool(path, L"Overlay", L"StartWithWindows", cfg.startWithWindows);
    WriteInt(path, L"Overlay", L"AsciiCellSize", cfg.asciiCellSize);
    WriteFloat(path, L"Effects", L"CrtStrength", cfg.crtStrength);
    WriteFloat(path, L"Effects", L"ScanlineStrength", cfg.scanlineStrength);
    WriteFloat(path, L"Effects", L"DitherStrength", cfg.ditherStrength);
    WriteFloat(path, L"Effects", L"GrainStrength", cfg.grainStrength);
    WriteFloat(path, L"Effects", L"GlowStrength", cfg.glowStrength);

    WriteInt(path, L"Leaves", L"Count", cfg.leafCount);
    WriteFloat(path, L"Leaves", L"SizeMin", cfg.leafSizeMin);
    WriteFloat(path, L"Leaves", L"SizeMax", cfg.leafSizeMax);
    WriteFloat(path, L"Leaves", L"Gravity", cfg.gravity);
    WriteFloat(path, L"Leaves", L"Drag", cfg.drag);
    WriteFloat(path, L"Leaves", L"BaseWind", cfg.baseWind);
    WriteFloat(path, L"Leaves", L"WindStrength", cfg.windStrength);
    WriteFloat(path, L"Leaves", L"Turbulence", cfg.turbulence);
    WriteFloat(path, L"Leaves", L"AnimationSpeed", cfg.animationSpeed);
    WriteFloat(path, L"Leaves", L"ReactionStrength", cfg.leafReactionStrength);
    WriteInt(path, L"Leaves", L"Palette", cfg.leafPalette);

    WriteFloat(path, L"Cursor", L"Radius", cfg.cursorRadius);
    WriteFloat(path, L"Cursor", L"Repulsion", cfg.cursorRepulsion);
    WriteFloat(path, L"Cursor", L"Vortex", cfg.cursorVortex);
    WriteFloat(path, L"Cursor", L"Wake", cfg.cursorWake);
    WriteFloat(path, L"Cursor", L"ForceSmoothing", cfg.cursorForceSmoothing);
    WriteFloat(path, L"Cursor", L"GustScale", cfg.cursorGustScale);
    WriteFloat(path, L"Cursor", L"MaxLeafSpeed", cfg.maxLeafSpeed);

    Logger::Instance().Info("Configuration saved");
    return true;
}

bool Configuration::LoadOrCreateDefaults(const std::wstring& path) {
    if (Load(path)) {
        // Persist migrations (e.g. v1 → v2 ambience defaults).
        Save(path);
        return true;
    }
    config_.ResetToDefaults();
    return Save(path);
}

bool SetStartWithWindows(bool enabled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        Logger::Instance().Error("Failed to open Run registry key");
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        std::wstring value = L"\"";
        value += modulePath;
        value += L"\"";
        result = RegSetValueExW(key, kAppName, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(value.c_str()),
                                static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kAppName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool IsStartWithWindowsEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    DWORD size = 0;
    const LONG result = RegQueryValueExW(key, kAppName, nullptr, &type, nullptr, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ;
}

} // namespace rjo
