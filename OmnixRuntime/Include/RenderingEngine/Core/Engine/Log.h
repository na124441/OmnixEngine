#pragma once
#include "RenderingEngine/core/log/Log.h"

// Map our Phase 0 macros to the existing engine logger (spdlog)
// We use "{}" to support the string concatenation style used in Phase 0 code.

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR
#undef LOG_FATAL

#define LOG_DEBUG(...) ENG_LOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)  ENG_LOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)  ENG_LOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) ENG_LOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...) { ENG_LOG_CRITICAL(__VA_ARGS__); std::abort(); }
