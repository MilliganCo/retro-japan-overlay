#include "MonitorManager.h"
#include "Utilities/Logger.h"

#include <ShellScalingApi.h>

#pragma comment(lib, "Shcore.lib")

namespace rjo {

void MonitorManager::Refresh() {
    monitors_.clear();
    EnumDisplayMonitors(nullptr, nullptr, EnumProc, reinterpret_cast<LPARAM>(this));

    virtualDesktop_.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    virtualDesktop_.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    virtualDesktop_.right = virtualDesktop_.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    virtualDesktop_.bottom = virtualDesktop_.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    char buf[128]{};
    sprintf_s(buf, "Monitors refreshed: count=%zu virtual=%dx%d",
              monitors_.size(),
              virtualDesktop_.right - virtualDesktop_.left,
              virtualDesktop_.bottom - virtualDesktop_.top);
    Logger::Instance().Info(buf);
}

SIZE MonitorManager::VirtualDesktopSize() const {
    return {
        virtualDesktop_.right - virtualDesktop_.left,
        virtualDesktop_.bottom - virtualDesktop_.top
    };
}

RECT MonitorManager::ResolveTargetRect(int monitorIndex) const {
    if (monitorIndex < 0 || monitors_.empty()) {
        return virtualDesktop_;
    }
    const int idx = ClampMonitorIndex(monitorIndex);
    return monitors_[static_cast<size_t>(idx)].rect;
}

int MonitorManager::ClampMonitorIndex(int monitorIndex) const {
    if (monitors_.empty()) {
        return -1;
    }
    if (monitorIndex < 0) {
        return -1;
    }
    if (monitorIndex >= static_cast<int>(monitors_.size())) {
        return static_cast<int>(monitors_.size()) - 1;
    }
    return monitorIndex;
}

BOOL CALLBACK MonitorManager::EnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* self = reinterpret_cast<MonitorManager*>(data);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(monitor, &mi)) {
        return TRUE;
    }

    MonitorInfo info;
    info.handle = monitor;
    info.rect = mi.rcMonitor;
    info.work = mi.rcWork;
    info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.name = mi.szDevice;

    UINT dpiX = 96, dpiY = 96;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        info.dpi = dpiX;
    }

    self->monitors_.push_back(std::move(info));
    return TRUE;
}

} // namespace rjo
