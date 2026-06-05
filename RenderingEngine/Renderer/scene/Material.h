#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include "Shader.h"
#include "Texture.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"

namespace eng::renderer {

/* Per‑material uniform data – can be expanded later */
struct MaterialUBO
{
    glm::vec4 albedoColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic  = 0.0f;
    float hasAlbedoMap = 0.0f;
    float hasNormalMap = 0.0f;
};

class Material
{
public:
    Material() = default;
    ~Material() { destroy(); }

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&&) noexcept = default;
    Material& operator=(Material&&) noexcept = default;

    /** Build a material from shader file paths and optional texture filenames.
     *  Returns false on any error (shader loading, pipeline creation, texture load…).
     */
    bool create(const std::string& vertPath,
                const std::string& fragPath,
                const std::string& albedoPath,   // empty string → use default white texture
                const std::string& normalPath,   // optional
                EngineResources& resources);

    /** Bind the pipeline **and** the material descriptor set (set = 1). */
    void bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout) const
    {
        if (pipelineHandle != VK_NULL_HANDLE) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineHandle);
        }
        if (descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout,
                                    1, // set index = 1 (global UBO is set 0)
                                    1, &descriptorSet,
                                    0, nullptr);
        }
    }

    VkPipeline pipeline() const { return pipelineHandle; }
    void setFallbackPipeline(VkPipeline p) { pipelineHandle = p; }

    /** Update the per‑material uniform buffer (e.g. roughness/metallic/albedoColor). */
    void setAlbedoColor(const glm::vec4& color) { uboData.albedoColor = color; if(resources) updateUniform(*resources); }
    glm::vec4 getAlbedoColor() const { return uboData.albedoColor; }
    void setRoughness(float r) { uboData.roughness = r; if(resources) updateUniform(*resources); }
    void setMetallic (float m) { uboData.metallic  = m; if(resources) updateUniform(*resources); }

    void setAlbedoTexture(std::shared_ptr<Texture> tex) { 
        albedoTexture = std::move(tex); 
        uboData.hasAlbedoMap = albedoTexture ? 1.0f : 0.0f;
        if(resources) updateUniform(*resources);
    }
    void setNormalTexture(std::shared_ptr<Texture> tex) { 
        normalTexture = std::move(tex); 
        uboData.hasNormalMap = normalTexture ? 1.0f : 0.0f;
        if(resources) updateUniform(*resources);
    }

    void destroy();

    // Public for ease of access by loaders/systems
    MaterialUBO         uboData{};
    std::shared_ptr<Texture> albedoTexture;   // shared ownership
    std::shared_ptr<Texture> normalTexture;   // optional
    bool                dirty = true;          // flag to trigger uniform upload

private:
    bool createPipeline(const EngineResources& resources);
    bool allocateDescriptorSet(const EngineResources& resources);
    void updateUniform(const EngineResources& resources);

    Shader               shader;          // holds vertex/fragment modules
    VkPipeline          pipelineHandle = VK_NULL_HANDLE;
    VkBuffer            uboBuffer = VK_NULL_HANDLE;
    VmaAllocation       uboAllocation = VK_NULL_HANDLE;
    VkDescriptorSet     descriptorSet = VK_NULL_HANDLE;
    EngineResources*    resources = nullptr;   // we keep a raw pointer for updates
};

} // namespace eng::renderer
