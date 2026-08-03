#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace eng::platform {

    struct DisplayMode {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t refreshRate = 0;
        uint32_t bitsPerPixel = 0;
    };

    struct MonitorInfo {
        std::string name;
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = 0;
        int32_t height = 0;
        int32_t workX = 0;
        int32_t workY = 0;
        int32_t workWidth = 0;
        int32_t workHeight = 0;
        int32_t refreshRate = 0;
        bool isPrimary = false;
    };

    class Monitor {
    public:
        /**
         * @brief Enumerate all connected monitors.
         * @return std::vector<MonitorInfo> listing details for all active monitors.
         */
        [[nodiscard]] static std::vector<MonitorInfo> Enumerate() noexcept;

        /**
         * @brief Query all supported display modes for a specific monitor.
         * @param monitorName The name of the monitor (e.g. from MonitorInfo::name).
         * @return std::vector<DisplayMode> listing all supported resolution and refresh rate configurations.
         */
        [[nodiscard]] static std::vector<DisplayMode> GetDisplayModes(const std::string& monitorName) noexcept;
    };

} // namespace eng::platform
