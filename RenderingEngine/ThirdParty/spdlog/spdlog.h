#pragma once
#include <iostream>
#include <string>
#include <memory>

#define SPDLOG_LEVEL_TRACE 0
#define SPDLOG_LEVEL_DEBUG 1
#define SPDLOG_LEVEL_INFO 2
#define SPDLOG_LEVEL_WARN 3
#define SPDLOG_LEVEL_ERROR 4
#define SPDLOG_LEVEL_CRITICAL 5
#define SPDLOG_LEVEL_OFF 6

namespace spdlog {
    namespace level {
        enum level_enum {
            trace = SPDLOG_LEVEL_TRACE,
            debug = SPDLOG_LEVEL_DEBUG,
            info = SPDLOG_LEVEL_INFO,
            warn = SPDLOG_LEVEL_WARN,
            err = SPDLOG_LEVEL_ERROR,
            critical = SPDLOG_LEVEL_CRITICAL,
            off = SPDLOG_LEVEL_OFF
        };
    }

    class logger {
    public:
        template<typename... Args> void trace(const char* fmt, const Args&... args) {}
        template<typename... Args> void debug(const char* fmt, const Args&... args) {}
        template<typename... Args> void info(const char* fmt, const Args&... args) { std::cout << "[INFO] " << fmt << std::endl; }
        template<typename... Args> void warn(const char* fmt, const Args&... args) { std::cout << "[WARN] " << fmt << std::endl; }
        template<typename... Args> void error(const char* fmt, const Args&... args) { std::cerr << "[ERROR] " << fmt << std::endl; }
        template<typename... Args> void critical(const char* fmt, const Args&... args) { std::cerr << "[CRITICAL] " << fmt << std::endl; }

        void set_level(level::level_enum l) {}
        void set_pattern(const std::string& p) {}
    };

    inline std::shared_ptr<logger> stdout_color_mt(const std::string& name) {
        return std::make_shared<logger>();
    }
}
