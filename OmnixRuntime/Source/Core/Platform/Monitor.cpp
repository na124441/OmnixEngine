#include "Core/Platform/Monitor.h"
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace eng::platform {

    namespace {
        BOOL CALLBACK MonitorEnumCallback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
            auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);

            MONITORINFOEXW info;
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(hMonitor, &info)) {
                MonitorInfo m;

                // Convert WideChar name to UTF-8
                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, info.szDevice, -1, nullptr, 0, nullptr, nullptr);
                if (utf8Len > 0) {
                    std::string name(utf8Len, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, info.szDevice, -1, &name[0], utf8Len, nullptr, nullptr);
                    if (!name.empty() && name.back() == '\0') {
                        name.pop_back();
                    }
                    m.name = name;
                } else {
                    m.name = "Unknown Monitor";
                }

                m.x = info.rcMonitor.left;
                m.y = info.rcMonitor.top;
                m.width = info.rcMonitor.right - info.rcMonitor.left;
                m.height = info.rcMonitor.bottom - info.rcMonitor.top;

                m.workX = info.rcWork.left;
                m.workY = info.rcWork.top;
                m.workWidth = info.rcWork.right - info.rcWork.left;
                m.workHeight = info.rcWork.bottom - info.rcWork.top;

                m.isPrimary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

                // Query current refresh rate
                DEVMODEW devMode = { 0 };
                devMode.dmSize = sizeof(devMode);
                if (EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
                    m.refreshRate = static_cast<int32_t>(devMode.dmDisplayFrequency);
                } else {
                    m.refreshRate = 60; // fallback
                }

                monitors->push_back(m);
            }
            return TRUE;
        }
    }

    std::vector<MonitorInfo> Monitor::Enumerate() noexcept {
        std::vector<MonitorInfo> monitors;
        EnumDisplayMonitors(nullptr, nullptr, MonitorEnumCallback, reinterpret_cast<LPARAM>(&monitors));
        return monitors;
    }

    std::vector<DisplayMode> Monitor::GetDisplayModes(const std::string& monitorName) noexcept {
        std::vector<DisplayMode> modes;

        int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, monitorName.c_str(), -1, nullptr, 0);
        std::wstring wideName(wideCharCount, 0);
        if (wideCharCount > 0) {
            MultiByteToWideChar(CP_UTF8, 0, monitorName.c_str(), -1, &wideName[0], wideCharCount);
            if (!wideName.empty() && wideName.back() == L'\0') {
                wideName.pop_back();
            }
        }

        DEVMODEW devMode = { 0 };
        devMode.dmSize = sizeof(devMode);

        DWORD modeIndex = 0;
        while (EnumDisplaySettingsW(wideName.empty() ? nullptr : wideName.c_str(), modeIndex, &devMode)) {
            DisplayMode mode;
            mode.width = static_cast<uint32_t>(devMode.dmPelsWidth);
            mode.height = static_cast<uint32_t>(devMode.dmPelsHeight);
            mode.refreshRate = static_cast<uint32_t>(devMode.dmDisplayFrequency);
            mode.bitsPerPixel = static_cast<uint32_t>(devMode.dmBitsPerPel);

            // Deduplicate
            bool duplicate = false;
            for (const auto& existing : modes) {
                if (existing.width == mode.width &&
                    existing.height == mode.height &&
                    existing.refreshRate == mode.refreshRate &&
                    existing.bitsPerPixel == mode.bitsPerPixel) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                modes.push_back(mode);
            }

            modeIndex++;
        }

        // Sort ascending
        std::sort(modes.begin(), modes.end(), [](const DisplayMode& a, const DisplayMode& b) {
            if (a.width != b.width) return a.width < b.width;
            if (a.height != b.height) return a.height < b.height;
            if (a.refreshRate != b.refreshRate) return a.refreshRate < b.refreshRate;
            return a.bitsPerPixel < b.bitsPerPixel;
        });

        return modes;
    }

} // namespace eng::platform

#else

namespace eng::platform {

    std::vector<MonitorInfo> Monitor::Enumerate() noexcept {
        // Fallback for non-Windows platforms
        MonitorInfo m;
        m.name = "Fallback Default Monitor";
        m.x = 0;
        m.y = 0;
        m.width = 1920;
        m.height = 1080;
        m.workX = 0;
        m.workY = 0;
        m.workWidth = 1920;
        m.workHeight = 1040;
        m.refreshRate = 60;
        m.isPrimary = true;
        return { m };
    }

    std::vector<DisplayMode> Monitor::GetDisplayModes(const std::string& /*monitorName*/) noexcept {
        return {
            { 1024, 768, 60, 32 },
            { 1280, 720, 60, 32 },
            { 1920, 1080, 60, 32 },
            { 1920, 1080, 120, 32 }
        };
    }

} // namespace eng::platform

#endif
