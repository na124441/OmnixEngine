#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "src/engine/Log.h"

inline void CheckVkResult(VkResult result, const char* call, const char* file, int line)
{
    if (result == VK_SUCCESS) return;

    // Convert to text (small helper)
    const char* msg = "";
    switch (result) {
        case VK_ERROR_OUT_OF_DATE_KHR: msg = "out of date"; break;
        case VK_ERROR_SURFACE_LOST_KHR: msg = "surface lost"; break;
        case VK_ERROR_DEVICE_LOST: msg = "device lost"; break;
        default: msg = "unknown error"; break;
    }

    std::string fullMsg = std::string(call) + " failed: " + msg;
    if (result == VK_ERROR_DEVICE_LOST) {
        LOG_FATAL(fullMsg + " – aborting.");
    } else {
        LOG_ERROR(fullMsg + " – aborting.");
    }
    // abort() called inside LOG_FATAL, otherwise you can decide to throw.
}

/* Macro makes usage terse */
#define VK_CHECK(call) CheckVkResult((call), #call, __FILE__, __LINE__)
