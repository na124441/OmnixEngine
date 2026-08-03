#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>
#include "Core/Engine/Log.h"

namespace eng {

inline void check_vk_result(VkResult err, const char* file, int line) {
    if (err == VK_SUCCESS) return;
    std::string msg = "Vulkan Error: " + std::to_string(err) + " at " + file + ":" + std::to_string(line);
    LOG_ERROR(msg);
    if (err < 0) {
        throw std::runtime_error(msg);
    }
}

} // namespace eng

#define VK_CHECK(x) ::eng::check_vk_result((x), __FILE__, __LINE__)
