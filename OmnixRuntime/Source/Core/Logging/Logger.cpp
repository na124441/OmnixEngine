#include "Core/Logging/Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <cstdlib>

namespace eng::logging {

    void Logger::Init(const std::string& filePath, LogSeverity minSeverity) {
        auto& state = GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        state.minSeverity = minSeverity;

        if (!filePath.empty()) {
            state.file.open(filePath, std::ios::out | std::ios::app);
            if (!state.file.is_open()) {
                std::cerr << "[Logger] Unable to open log file: " << filePath << std::endl;
            }
        }

        LogWrite(LogSeverity::Info, LogCategory::Runtime, "Logger initialised");
    }

    void Logger::Init(const std::string& filePath, ::LogLevel minLevel) {
        LogSeverity severity = LogSeverity::Info;
        switch (minLevel) {
            case LogLevel::Trace: severity = LogSeverity::Trace; break;
            case LogLevel::Debug: severity = LogSeverity::Trace; break;
            case LogLevel::Info:  severity = LogSeverity::Info; break;
            case LogLevel::Warn:  severity = LogSeverity::Warning; break;
            case LogLevel::Error: severity = LogSeverity::Error; break;
            case LogLevel::Fatal: severity = LogSeverity::Fatal; break;
            case LogLevel::Off:   severity = LogSeverity(5); break; // Off mapped to 5
        }
        Init(filePath, severity);
    }

    void Logger::Shutdown() {
        auto& state = GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        LogWrite(LogSeverity::Info, LogCategory::Runtime, "Logger shutting down");
        if (state.file.is_open()) {
            state.file.flush();
            state.file.close();
        }
    }

    void Logger::SetSeverity(LogSeverity severity) {
        auto& state = GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        state.minSeverity = severity;
    }

    LogSeverity Logger::GetSeverity() {
        auto& state = GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        return state.minSeverity;
    }

    void Logger::LogInternal(LogSeverity severity, LogCategory category, const std::string& message) {
        auto& state = GetState();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);

        std::string timestamp = GetFormattedTimestamp();
        const char* severityStr = LogSeverityToString(severity);
        const char* categoryStr = LogCategoryToString(category);

        // Format: [timestamp][Category][Severity] Message
        std::ostringstream entry;
        entry << timestamp << "[" << categoryStr << "][" << severityStr << "] " << message;

        // Print to console
        if (severity == LogSeverity::Error || severity == LogSeverity::Fatal) {
            std::cerr << entry.str() << std::endl;
        } else {
            std::cout << entry.str() << std::endl;
        }

        // Print to file
        if (state.file.is_open()) {
            state.file << entry.str() << std::endl;
            state.file.flush();
        }

        // Fatal exits the application
        if (severity == LogSeverity::Fatal) {
            std::cerr << "[Logger] Fatal error encountered - aborting execution." << std::endl;
            std::abort();
        }
    }

    std::string Logger::GetFormattedTimestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = system_clock::to_time_t(now);
        
        std::tm bt;
#if defined(_WIN32)
        localtime_s(&bt, &timer);
#else
        localtime_r(&bt, &timer);
#endif

        std::ostringstream oss;
        oss << "[" << std::setfill('0')
            << std::setw(2) << bt.tm_hour << ":"
            << std::setw(2) << bt.tm_min << ":"
            << std::setw(2) << bt.tm_sec << "."
            << std::setw(3) << ms.count() << "]";
        return oss.str();
    }

} // namespace eng::logging
