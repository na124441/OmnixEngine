#pragma once
#include <atomic>
#include <cassert>
#include <string>
#include "Core/Engine/Log.h"

struct ResourceTracker {
    static std::atomic<int64_t> aliveBuffers;
    static std::atomic<int64_t> aliveImages;

    static void incBuffer() { ++aliveBuffers; }
    static void decBuffer() { --aliveBuffers; }
    static void incImage()  { ++aliveImages;  }
    static void decImage()  { --aliveImages;   }

    static void validateAtShutdown()
    {
        if (aliveBuffers != 0 || aliveImages != 0) {
            LOG_FATAL("Resource leak detected: "
                      "Buffers=" + std::to_string(aliveBuffers.load()) +
                      " Images="  + std::to_string(aliveImages.load()));
        } else {
            LOG_INFO("All Vulkan resources properly destroyed.");
        }
    }
};

/* Definition (in a .cpp or inline) */
inline std::atomic<int64_t> ResourceTracker::aliveBuffers{0};
inline std::atomic<int64_t> ResourceTracker::aliveImages{0};
