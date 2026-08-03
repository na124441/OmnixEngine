#pragma once
#include <vulkan/vulkan.h>

namespace eng {

class DebugLabel {
public:
    static void Init(VkInstance instance);
    static void Begin(VkCommandBuffer cmd, const char* name, float color[4] = nullptr);
    static void End(VkCommandBuffer cmd);
    static void Insert(VkCommandBuffer cmd, const char* name, float color[4] = nullptr);
};

} // namespace eng
