#include "Core/pch.h"
#include "RenderingEngine/Renderer/scene/Shader.h"
#include <fstream>
#include <filesystem>

namespace eng::renderer {

// ---------------------------------------------------------------------
// Helper – read whole binary file into a vector<char>
static std::vector<char> readFile(const std::string& filename)
{
    LOG_INFO("Attempting to open shader: " + filename);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::string absPath = std::filesystem::absolute(filename).string();
        LOG_ERROR("Failed to open shader file: " + filename + " (Resolved to: " + absPath + ")");
        return {};
    }
    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    return buffer;
}

// ---------------------------------------------------------------------
bool Shader::load(const std::string& vertPath,
                  const std::string& fragPath,
                  VkDevice dev)
{
    device = dev;

    // ---- Vertex shader -------------------------------------------------
    auto vertCode = readFile(vertPath);
    if (vertCode.empty()) return false;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = vertCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(vertCode.data());

    VkResult r = vkCreateShaderModule(device, &createInfo, nullptr, &vert);
    VK_CHECK(r);
    if (r != VK_SUCCESS) return false;

    // ---- Fragment shader -----------------------------------------------
    auto fragCode = readFile(fragPath);
    if (fragCode.empty()) {
        vkDestroyShaderModule(device, vert, nullptr);
        vert = VK_NULL_HANDLE;
        return false;
    }
    createInfo.codeSize = fragCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(fragCode.data());

    r = vkCreateShaderModule(device, &createInfo, nullptr, &frag);
    VK_CHECK(r);
    if (r != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert, nullptr);
        vert = VK_NULL_HANDLE;
        return false;
    }

    LOG_INFO("Shader modules loaded: " + vertPath + " + " + fragPath);
    return true;
}

// ---------------------------------------------------------------------
void Shader::destroy()
{
    if (vert != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, vert, nullptr);
        vert = VK_NULL_HANDLE;
    }
    if (frag != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, frag, nullptr);
        frag = VK_NULL_HANDLE;
    }
}

} // namespace eng::renderer
