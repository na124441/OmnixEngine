#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "Core/Engine/Log.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/VmaHelpers.h"

namespace eng::renderer {

/// Minimal description of a descriptor that a shader expects.
struct DescriptorInfo
{
    uint32_t          binding;   // binding number in the shader
    VkDescriptorType  type;      // UNIFORM_BUFFER, COMBINED_IMAGE_SAMPLER, …
    VkShaderStageFlags stage;   // which stage uses it
};

class Shader
{
public:
    Shader() = default;
    ~Shader() { destroy(); }

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) noexcept = default;
    Shader& operator=(Shader&&) noexcept = default;

    /// Load vertex & fragment SPIR‑V modules.
    /// Returns false on failure (file missing, vkCreateShaderModule error, …)
    bool load(const std::string& vertPath,
              const std::string& fragPath,
              VkDevice device);

    VkShaderModule   vertModule() const { return vert; }
    VkShaderModule   fragModule() const { return frag; }
    std::vector<DescriptorInfo>& getDescriptorInfos() { return descriptorInfos; }
    const std::vector<DescriptorInfo>& getDescriptorInfos() const { return descriptorInfos; }

    void destroy();

private:
    VkDevice          device = VK_NULL_HANDLE;
    VkShaderModule    vert   = VK_NULL_HANDLE;
    VkShaderModule    frag   = VK_NULL_HANDLE;
    std::vector<DescriptorInfo> descriptorInfos;
};

} // namespace eng::renderer
