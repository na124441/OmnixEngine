#pragma once

namespace eng::logging {

    enum class LogSeverity {
        Trace,
        Info,
        Warning,
        Error,
        Fatal
    };

    inline const char* LogSeverityToString(LogSeverity severity) {
        switch (severity) {
            case LogSeverity::Trace:   return "Trace";
            case LogSeverity::Info:    return "Info";
            case LogSeverity::Warning: return "Warning";
            case LogSeverity::Error:   return "Error";
            case LogSeverity::Fatal:   return "Fatal";
            default:                   return "Unknown";
        }
    }

} // namespace eng::logging
