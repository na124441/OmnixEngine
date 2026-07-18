#pragma once
#define VK_ENABLE_BETA_EXTENSIONS // needed for some drivers (optional)

#include <vulkan/vulkan.h>
#include <cstring>
#include <string>
#include "src/engine/Log.h"

class DebugLabel {
public:
    static void Init(VkInstance instance)
    {
        // Load function pointers once
        vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
        vkCmdEndDebugUtilsLabelEXT   = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
        vkCmdInsertDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
        if (!vkCmdBeginDebugUtilsLabelEXT || !vkCmdEndDebugUtilsLabelEXT) {
            LOG_WARN("Debug Utils not available – RenderDoc markers disabled.");
            enabled = false;
        } else {
            enabled = true;
        }
    }

    static bool IsEnabled() { return enabled; }

    static void Begin(VkCommandBuffer cmd, const char* name, const float color[4] = nullptr)
    {
        if (!enabled) return;
        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        if (color) {
            memcpy(label.color, color, sizeof(label.color));
        } else {
            // default bright cyan for visibility
            label.color[0] = 0.0f;
            label.color[1] = 1.0f;
            label.color[2] = 1.0f;
            label.color[3] = 1.0f;
        }
        vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
    }

    static void End(VkCommandBuffer cmd)
    {
        if (!enabled) return;
        vkCmdEndDebugUtilsLabelEXT(cmd);
    }

    static void Insert(VkCommandBuffer cmd, const char* name, const float color[4] = nullptr)
    {
        if (!enabled) return;
        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        if (color) memcpy(label.color, color, sizeof(label.color));
        vkCmdInsertDebugUtilsLabelEXT(cmd, &label);
    }

private:
    static PFN_vkCmdBeginDebugUtilsLabelEXT  vkCmdBeginDebugUtilsLabelEXT;
    static PFN_vkCmdEndDebugUtilsLabelEXT    vkCmdEndDebugUtilsLabelEXT;
    static PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
    static bool enabled;
};
