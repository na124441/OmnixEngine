// Log.h
#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

namespace eng {

    enum class LogLevel {
        Trace = SPDLOG_LEVEL_TRACE,
        Debug = SPDLOG_LEVEL_DEBUG,
        Info = SPDLOG_LEVEL_INFO,
        Warn = SPDLOG_LEVEL_WARN,
        Err = SPDLOG_LEVEL_ERROR,
        Critical = SPDLOG_LEVEL_CRITICAL,
        Off = SPDLOG_LEVEL_OFF
    };

    class Log {
    public:
        // Initialise once at engine start (called by EngineLoop)
        static void Init(LogLevel level = LogLevel::Info) {
            if (!logger_) {
                logger_ = spdlog::stdout_color_mt("eng");
                logger_->set_level(static_cast<spdlog::level::level_enum>(level));
                logger_->set_pattern("[%T.%e] [%^%l%$] [%n] %v");
            }
        }

        static void Shutdown() {
            logger_.reset();
        }

        // Direct access – rarely needed, but useful for custom sinks
        static std::shared_ptr<spdlog::logger> Get() { return logger_; }

    private:
        inline static std::shared_ptr<spdlog::logger> logger_;
    };

} // namespace eng

#include "Core/Logger.h"

namespace eng {
    namespace detail {
        template<typename... Args>
        void LogGeneric(::LogLevel level, spdlog::level::level_enum spdLevel, const char* fmt, Args&&... args) {
            if (auto l = Log::Get()) {
                switch (spdLevel) {
                    case spdlog::level::trace:    l->trace(fmt, std::forward<Args>(args)...); break;
                    case spdlog::level::debug:    l->debug(fmt, std::forward<Args>(args)...); break;
                    case spdlog::level::info:     l->info(fmt, std::forward<Args>(args)...); break;
                    case spdlog::level::warn:     l->warn(fmt, std::forward<Args>(args)...); break;
                    case spdlog::level::err:      l->error(fmt, std::forward<Args>(args)...); break;
                    case spdlog::level::critical: l->critical(fmt, std::forward<Args>(args)...); break;
                    default: break;
                }
            } else {
                ::Logger::Logf(level, fmt, std::forward<Args>(args)...);
            }
        }

        inline void LogString(::LogLevel level, spdlog::level::level_enum spdLevel, const std::string& msg) {
            if (auto l = Log::Get()) {
                const char* cstr = msg.c_str();
                switch (spdLevel) {
                    case spdlog::level::trace:    l->trace(cstr); break;
                    case spdlog::level::debug:    l->debug(cstr); break;
                    case spdlog::level::info:     l->info(cstr); break;
                    case spdlog::level::warn:     l->warn(cstr); break;
                    case spdlog::level::err:      l->error(cstr); break;
                    case spdlog::level::critical: l->critical(cstr); break;
                    default: break;
                }
            } else {
                ::Logger::Log(level, msg);
            }
        }
    }

    // Overloaded helpers for all levels
    inline void LogTrace(const std::string& m) { detail::LogString(::LogLevel::Trace, spdlog::level::trace, m); }
    template<typename... Args> void LogTrace(const char* f, Args&&... a) { detail::LogGeneric(::LogLevel::Trace, spdlog::level::trace, f, std::forward<Args>(a)...); }

    inline void LogDebug(const std::string& m) { detail::LogString(::LogLevel::Debug, spdlog::level::debug, m); }
    template<typename... Args> void LogDebug(const char* f, Args&&... a) { detail::LogGeneric(::LogLevel::Debug, spdlog::level::debug, f, std::forward<Args>(a)...); }

    inline void LogInfo(const std::string& m) { detail::LogString(::LogLevel::Info, spdlog::level::info, m); }
    template<typename... Args> void LogInfo(const char* f, Args&&... a) { detail::LogGeneric(::LogLevel::Info, spdlog::level::info, f, std::forward<Args>(a)...); }

    inline void LogWarn(const std::string& m) { detail::LogString(::LogLevel::Warn, spdlog::level::warn, m); }
    template<typename... Args> void LogWarn(const char* f, Args&&... a) { detail::LogGeneric(::LogLevel::Warn, spdlog::level::warn, f, std::forward<Args>(a)...); }

    inline void LogError(const std::string& m) { detail::LogString(::LogLevel::Error, spdlog::level::err, m); }
    template<typename... Args> void LogError(const char* f, Args&&... a) { detail::LogGeneric(::LogLevel::Error, spdlog::level::err, f, std::forward<Args>(a)...); }

    inline void LogCritical(const std::string& m) { detail::LogString(::LogLevel::Fatal, spdlog::level::critical, m); }
    template<typename... Args> void LogCritical(const char* f, Args&&... a) { detail::LogGeneric(::LogLevel::Fatal, spdlog::level::critical, f, std::forward<Args>(a)...); }
}

// Convenience wrappers mapped to helpers
#define ENG_LOG_TRACE(...)   ::eng::LogTrace(__VA_ARGS__)
#define ENG_LOG_DEBUG(...)   ::eng::LogDebug(__VA_ARGS__)
#define ENG_LOG_INFO(...)    ::eng::LogInfo(__VA_ARGS__)
#define ENG_LOG_WARN(...)    ::eng::LogWarn(__VA_ARGS__)
#define ENG_LOG_ERROR(...)   ::eng::LogError(__VA_ARGS__)
#define ENG_LOG_CRITICAL(...) ::eng::LogCritical(__VA_ARGS__)

