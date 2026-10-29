#pragma once

#include <Windows.h>
#include <vector>
#include <string>

namespace rjo {

struct MonitorInfo {
    HMONITOR handle = nullptr;
    RECT rect{};
    RECT work{};
    UINT dpi = 96;
    std::wstring name;
    bool primary = false;
};

class MonitorManager {
public:
    void Refresh();

    const std::vector<MonitorInfo>& Monitors() const { return monitors_; }
    RECT VirtualDesktop() const { return virtualDesktop_; }
    SIZE VirtualDesktopSize() const;

    // monitorIndex < 0 => full virtual desktop
    RECT ResolveTargetRect(int monitorIndex) const;
    int ClampMonitorIndex(int monitorIndex) const;

private:
    static BOOL CALLBACK EnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM data);

    std::vector<MonitorInfo> monitors_;
    RECT virtualDesktop_{};
};

} // namespace rjo
