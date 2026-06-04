#pragma once

#include "Core/Logging/Logger.h"
#include <iostream>
#include <cstdlib>

#define OMNIX_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            LOG_ERROR("Assertion failed: (%s) - %s", #condition, message); \
        } \
    } while (0)

#define OMNIX_FATAL_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            LOG_FATAL("Fatal Assertion failed: (%s) - %s", #condition, message); \
            std::cerr << "Fatal Assertion failed: (" << #condition << ") - " << message << std::endl; \
            std::abort(); \
        } \
    } while (0)
