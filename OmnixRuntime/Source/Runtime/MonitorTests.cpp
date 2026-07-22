#include "Runtime/MonitorTests.h"
#include "Core/Platform/Monitor.h"
#include "Core/Logger.h"
#include <cassert>

namespace eng::runtime {

    void RunMonitorTests() {
        CORE_LOG_INFO("=== Running Kernel Monitor Enumeration Tests ===");

        CORE_LOG_INFO("[Test] Querying connected monitors...");
        auto monitors = eng::platform::Monitor::Enumerate();

        assert(!monitors.empty() && "No connected monitors detected!");
        CORE_LOG_INFO("[Test] Found %zu connected monitor(s):", monitors.size());

        bool hasPrimary = false;
        for (size_t i = 0; i < monitors.size(); ++i) {
            const auto& m = monitors[i];
            CORE_LOG_INFO("  Monitor #%zu: '%s'", i, m.name.c_str());
            CORE_LOG_INFO("    Bounds:    [x: %d, y: %d] size: %dx%d", m.x, m.y, m.width, m.height);
            CORE_LOG_INFO("    Work Area: [x: %d, y: %d] size: %dx%d", m.workX, m.workY, m.workWidth, m.workHeight);
            CORE_LOG_INFO("    Refresh:   %d Hz", m.refreshRate);
            CORE_LOG_INFO("    Primary:   %s", m.isPrimary ? "Yes" : "No");

            // Check basic bounds constraints
            assert(m.width > 0 && m.height > 0 && "Invalid monitor dimensions!");
            assert(m.workWidth > 0 && m.workHeight > 0 && "Invalid work area dimensions!");
            assert(m.refreshRate > 0 && "Invalid refresh rate!");

            // Test Display Modes query
            auto modes = eng::platform::Monitor::GetDisplayModes(m.name);
            assert(!modes.empty() && "No display modes returned for active monitor!");
            CORE_LOG_INFO("    Display Modes: %zu modes found (showing up to 5)", modes.size());

            bool currentModeMatched = false;
            size_t showCount = modes.size() < 5 ? modes.size() : 5;
            for (size_t j = 0; j < modes.size(); ++j) {
                const auto& dm = modes[j];
                if (j < showCount) {
                    CORE_LOG_INFO("      Mode #%zu: %dx%d @ %d Hz (%d bpp)", j, dm.width, dm.height, dm.refreshRate, dm.bitsPerPixel);
                }
                if (dm.width == static_cast<uint32_t>(m.width) &&
                    dm.height == static_cast<uint32_t>(m.height)) {
                    currentModeMatched = true;
                }
            }
            assert(currentModeMatched && "Current monitor resolution not found in supported display modes list!");

            if (m.isPrimary) {
                hasPrimary = true;
            }
        }

        assert(hasPrimary && "No primary monitor detected in system enumeration list!");
        CORE_LOG_INFO("[Test] Monitor attributes and primary flag validated successfully.");

        CORE_LOG_INFO("=== All Monitor Enumeration Tests Passed Successfully ===");
    }
}
