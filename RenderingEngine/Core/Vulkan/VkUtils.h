#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>

#ifndef VK_CHECK
#define VK_CHECK(x)                                                                         \
    do {                                                                                    \
        VkResult err = x;                                                                  \
        if (err != VK_SUCCESS) {                                                            \
            throw std::runtime_error("Vulkan error at " + std::string(__FILE__) + ":" +     \
                                     std::to_string(__LINE__) + " - Result: " +             \
                                     std::to_string(err));                                  \
        }                                                                                   \
    } while (0)
#endif
