#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>

namespace eng::renderer {

    class ShaderLibrary {
    public:
        static VkShaderModule LoadShader(VkDevice device, const std::string& path);
        static VkShaderModule ReloadShader(VkDevice device, const std::string& path);
        static void ClearCache(VkDevice device);

    private:
        static std::unordered_map<std::string, VkShaderModule> m_Cache;
    };

} // namespace eng::renderer
