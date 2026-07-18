#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include "RenderingEngine/Renderer/scene/Shader.h"
#include "RenderingEngine/Renderer/scene/Texture.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"
#include "Rendering/Materials/MaterialAsset.h"

namespace eng::renderer {

class Mesh;

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

    /** Build a material using all 5 PBR texture paths and material asset parameters. */
    bool createPBR(const std::string& vertPath,
                   const std::string& fragPath,
                   const MaterialAsset& asset,
                   const std::string& albedoPath,
                   const std::string& normalPath,
                   const std::string& metallicRoughnessPath,
                   const std::string& aoPath,
                   const std::string& emissivePath,
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

    /** Bind only the material descriptor set (set = 1 by default). */
    void bindDescriptorSet(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t setIndex = 1) const
    {
        if (descriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout,
                                    setIndex,
                                    1, &descriptorSet,
                                    0, nullptr);
        }
    }

    VkPipeline pipeline() const { return pipelineHandle; }
    void setFallbackPipeline(VkPipeline p) { pipelineHandle = p; }

    // Factor Getters/Setters that sync with both assetData and uboData
    void setAlbedoColor(const glm::vec4& color) {
        assetData.baseColorFactor = color;
        uboData.baseColorFactor = color;
        if(resources) updateUniform(*resources);
    }
    glm::vec4 getAlbedoColor() const { return assetData.baseColorFactor; }

    void setRoughness(float r) {
        assetData.roughnessFactor = r;
        uboData.roughnessFactor = r;
        if(resources) updateUniform(*resources);
    }
    float getRoughness() const { return assetData.roughnessFactor; }

    void setMetallic(float m) {
        assetData.metallicFactor = m;
        uboData.metallicFactor = m;
        if(resources) updateUniform(*resources);
    }
    float getMetallic() const { return assetData.metallicFactor; }

    void setNormalScale(float s) {
        assetData.normalScale = s;
        uboData.normalScale = s;
        if(resources) updateUniform(*resources);
    }
    float getNormalScale() const { return assetData.normalScale; }

    void setEmissiveStrength(float e) {
        assetData.emissiveStrength = e;
        uboData.emissiveStrength = e;
        if(resources) updateUniform(*resources);
    }
    float getEmissiveStrength() const { return assetData.emissiveStrength; }

    void setClearcoatFactor(float cf) {
        assetData.clearcoatFactor = cf;
        uboData.clearcoatFactor = cf;
        if(resources) updateUniform(*resources);
    }
    float getClearcoatFactor() const { return assetData.clearcoatFactor; }

    void setClearcoatRoughness(float cr) {
        assetData.clearcoatRoughness = cr;
        uboData.clearcoatRoughness = cr;
        if(resources) updateUniform(*resources);
    }
    float getClearcoatRoughness() const { return assetData.clearcoatRoughness; }

    void setBlendMode(MaterialBlendMode mode) {
        assetData.blendMode = mode;
        uboData.blendMode = static_cast<uint32_t>(mode);
        if(resources) updateUniform(*resources);
    }
    MaterialBlendMode getBlendMode() const { return assetData.blendMode; }

    void setShadingModel(MaterialShadingModel model) {
        assetData.shadingModel = model;
        uboData.shadingModel = static_cast<uint32_t>(model);
        if(resources) updateUniform(*resources);
    }
    MaterialShadingModel getShadingModel() const { return assetData.shadingModel; }

    void setAlbedoTexture(std::shared_ptr<Texture> tex) { 
        albedoTexture = std::move(tex); 
        uboData.hasAlbedoMap = albedoTexture ? 1.0f : 0.0f;
        if(resources) updateUniform(*resources);
    }
    void setNormalTexture(std::shared_ptr<Texture> tex) { 
        normalTexture = std::move(tex); 
        uboData.useNormalMap = normalTexture ? 1.0f : 0.0f;
        if(resources) updateUniform(*resources);
    }

    void updateNormalMapCompatibility(const Mesh& mesh);
    void setMetallicRoughnessTexture(std::shared_ptr<Texture> tex) { 
        metallicRoughnessTexture = std::move(tex); 
        uboData.hasMetallicRoughnessMap = metallicRoughnessTexture ? 1.0f : 0.0f;
        if(resources) updateUniform(*resources);
    }
    void setAOTexture(std::shared_ptr<Texture> tex) { 
        aoTexture = std::move(tex); 
        uboData.hasAOMap = aoTexture ? 1.0f : 0.0f;
        if(resources) updateUniform(*resources);
    }
    void setEmissiveTexture(std::shared_ptr<Texture> tex) { 
        emissiveTexture = std::move(tex); 
        uboData.hasEmissiveMap = emissiveTexture ? 1.0f : 0.0f;
        if(resources) updateUniform(*resources);
    }

    void destroy();

    // Public for ease of access by loaders/systems
    MaterialGPU         uboData{};
    MaterialAsset       assetData{};
    std::shared_ptr<Texture> albedoTexture;
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> metallicRoughnessTexture;
    std::shared_ptr<Texture> aoTexture;
    std::shared_ptr<Texture> emissiveTexture;
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
