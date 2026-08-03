#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>

enum class LogLevel { DEBUG = 0, INFO, WARN, ERROR, FATAL };

#ifndef ENGINE_LOG_LEVEL
#   define ENGINE_LOG_LEVEL LogLevel::DEBUG   // change at compile time
#endif

inline const char* toString(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:               return "UNKNOWN";
    }
}

inline void engineLog(LogLevel lvl,
                     const char* file,
                     int line,
                     const std::string& msg)
{
    if (static_cast<int>(lvl) < static_cast<int>(ENGINE_LOG_LEVEL)) return;

    std::cerr << "[" << toString(lvl) << "] "
              << std::setw(20) << std::left << file << ":" << line << "  "
              << msg << std::endl;

    if (lvl == LogLevel::FATAL) std::abort();
}

/* Macros – capture source location automatically */
#define LOG_DEBUG(msg)  engineLog(LogLevel::DEBUG, __FILE__, __LINE__, (msg))
#define LOG_INFO(msg)   engineLog(LogLevel::INFO,  __FILE__, __LINE__, (msg))
#define LOG_WARN(msg)   engineLog(LogLevel::WARN,  __FILE__, __LINE__, (msg))
#define LOG_ERROR(msg)  engineLog(LogLevel::ERROR, __FILE__, __LINE__, (msg))
#define LOG_FATAL(msg)  engineLog(LogLevel::FATAL, __FILE__, __LINE__, (msg))
