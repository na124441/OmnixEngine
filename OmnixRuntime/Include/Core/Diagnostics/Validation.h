#pragma once

#include "Core/Diagnostics/Assert.h"
#include <string>

// Null pointer check
#define OMNIX_VALIDATE_PTR(ptr) \
    do { \
        OMNIX_FATAL_ASSERT((ptr) != nullptr, "Null pointer validation failed: " #ptr " must not be null."); \
    } while (0)

// Standard condition validation
#define OMNIX_VALIDATE(condition, message) \
    do { \
        OMNIX_FATAL_ASSERT((condition), "Validation failed: " message); \
    } while (0)

// GPU Vulkan validation (0 matches VK_SUCCESS)
#define OMNIX_VALIDATE_VK(vkCall) \
    do { \
        int res = static_cast<int>(vkCall); \
        OMNIX_FATAL_ASSERT(res == 0, "Vulkan validation failed for: " #vkCall " (Result code: " + std::to_string(res) + ")"); \
    } while (0)
