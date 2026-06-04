#pragma once

#include "Core/Logging/LogCategory.h"
#include "Core/Logging/LogSeverity.h"
#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <cstdio>
#include <utility>

// Keep LogLevel for backward compatibility
enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off
};

namespace eng::logging {

    class Logger {
    public:
        static void Init(const std::string& filePath = "", LogSeverity minSeverity = LogSeverity::Info);
        static void Init(const std::string& filePath, ::LogLevel minLevel);

        static void Shutdown();

        // Template overloads for category-based logging
        template<typename... Args>
        static void LogWrite(LogSeverity severity, LogCategory category, const char* fmt, Args&&... args) {
            auto& state = GetState();
            if (severity < state.minSeverity || state.minSeverity == LogSeverity(5)) // Off mapped to 5
                return;

            // Compute required buffer size.
            const int size = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...) + 1;
            if (size <= 0) return;

            auto buf = std::make_unique<char[]>(size);
            std::snprintf(buf.get(), size, fmt, std::forward<Args>(args)...);
            LogInternal(severity, category, std::string(buf.get(), buf.get() + size - 1));
        }

        // Template overloads for backward compatibility logging (without category)
        template<typename... Args>
        static void LogWrite(LogSeverity severity, const char* fmt, Args&&... args) {
            LogWrite(severity, LogCategory::Runtime, fmt, std::forward<Args>(args)...);
        }

        static void SetSeverity(LogSeverity severity);
        static LogSeverity GetSeverity();

    private:
        struct LoggerState {
            std::ofstream file;
            LogSeverity minSeverity = LogSeverity::Info;
            std::recursive_mutex mutex;
        };

        static LoggerState& GetState() {
            static LoggerState state;
            return state;
        }

        static void LogInternal(LogSeverity severity, LogCategory category, const std::string& message);
        static std::string GetFormattedTimestamp();
    };

} // namespace eng::logging

// Shim class to keep old calls like `Logger::Init` or `Logger::Shutdown` compiling
class Logger {
public:
    static void Init(const std::string& filePath = "", LogLevel minLevel = LogLevel::Info) {
        ::eng::logging::Logger::Init(filePath, minLevel);
    }
    static void Shutdown() {
        ::eng::logging::Logger::Shutdown();
    }
    static void Log(LogLevel level, const std::string& message) {
        ::eng::logging::LogSeverity severity = ::eng::logging::LogSeverity::Info;
        switch (level) {
            case LogLevel::Trace: severity = ::eng::logging::LogSeverity::Trace; break;
            case LogLevel::Debug: severity = ::eng::logging::LogSeverity::Trace; break;
            case LogLevel::Info:  severity = ::eng::logging::LogSeverity::Info; break;
            case LogLevel::Warn:  severity = ::eng::logging::LogSeverity::Warning; break;
            case LogLevel::Error: severity = ::eng::logging::LogSeverity::Error; break;
            case LogLevel::Fatal: severity = ::eng::logging::LogSeverity::Fatal; break;
            case LogLevel::Off:   return;
        }
        ::eng::logging::Logger::LogWrite(severity, "%s", message.c_str());
    }
    template<typename... Args>
    static void Logf(LogLevel level, const char* fmt, Args&&... args) {
        ::eng::logging::LogSeverity severity = ::eng::logging::LogSeverity::Info;
        switch (level) {
            case LogLevel::Trace: severity = ::eng::logging::LogSeverity::Trace; break;
            case LogLevel::Debug: severity = ::eng::logging::LogSeverity::Trace; break;
            case LogLevel::Info:  severity = ::eng::logging::LogSeverity::Info; break;
            case LogLevel::Warn:  severity = ::eng::logging::LogSeverity::Warning; break;
            case LogLevel::Error: severity = ::eng::logging::LogSeverity::Error; break;
            case LogLevel::Fatal: severity = ::eng::logging::LogSeverity::Fatal; break;
            case LogLevel::Off:   return;
        }
        ::eng::logging::Logger::LogWrite(severity, fmt, std::forward<Args>(args)...);
    }
};

// Convenience macros mapping to Logger::LogWrite
#define CORE_LOG_TRACE(...) ::eng::logging::Logger::LogWrite(::eng::logging::LogSeverity::Trace, __VA_ARGS__)
#define CORE_LOG_DEBUG(...) ::eng::logging::Logger::LogWrite(::eng::logging::LogSeverity::Trace, __VA_ARGS__)
#define CORE_LOG_INFO(...)  ::eng::logging::Logger::LogWrite(::eng::logging::LogSeverity::Info, __VA_ARGS__)
#define CORE_LOG_WARN(...)  ::eng::logging::Logger::LogWrite(::eng::logging::LogSeverity::Warning, __VA_ARGS__)
#define CORE_LOG_ERROR(...) ::eng::logging::Logger::LogWrite(::eng::logging::LogSeverity::Error, __VA_ARGS__)
#define CORE_LOG_FATAL(...) ::eng::logging::Logger::LogWrite(::eng::logging::LogSeverity::Fatal, __VA_ARGS__)

#ifndef OMNIX_DONT_DEFINE_GLOBAL_LOG_MACROS
#define LOG_TRACE(...) CORE_LOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) CORE_LOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)  CORE_LOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)  CORE_LOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) CORE_LOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...) CORE_LOG_FATAL(__VA_ARGS__)
#endif
