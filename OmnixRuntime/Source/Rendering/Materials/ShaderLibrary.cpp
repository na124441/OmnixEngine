#include "Core/pch.h"
#include "Rendering/Materials/ShaderLibrary.h"
#include <fstream>
#include <vector>

namespace eng::renderer {

std::unordered_map<std::string, VkShaderModule> ShaderLibrary::m_Cache;

static std::vector<char> ReadFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule ShaderLibrary::LoadShader(VkDevice device, const std::string& path) {
    auto it = m_Cache.find(path);
    if (it != m_Cache.end()) {
        return it->second;
    }

    std::vector<char> code = ReadFile(path);
    if (code.empty()) {
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    m_Cache[path] = shaderModule;
    return shaderModule;
}

void ShaderLibrary::ClearCache(VkDevice device) {
    for (auto& pair : m_Cache) {
        if (pair.second != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, pair.second, nullptr);
        }
    }
    m_Cache.clear();
}

} // namespace eng::renderer
