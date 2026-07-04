#include "Core/pch.h"
#include "Material.h"
#include "Mesh.h"
#include "Core/types/Vertex.h"
#include "Rendering/Core/Renderer.h"
#include <cstddef>
#include <cstring>

namespace eng::renderer {

bool Material::create(const std::string& vertPath,
                     const std::string& fragPath,
                     const std::string& albedoPath,
                     const std::string& normalPath,
                     EngineResources& res)
{
    MaterialAsset asset{};
    return createPBR(vertPath, fragPath, asset, albedoPath, normalPath, "", "", "", res);
}

bool Material::createPBR(const std::string& vertPath,
                         const std::string& fragPath,
                         const MaterialAsset& asset,
                         const std::string& albedoPath,
                         const std::string& normalPath,
                         const std::string& metallicRoughnessPath,
                         const std::string& aoPath,
                         const std::string& emissivePath,
                         EngineResources& res)
{
    resources = &res;   // store for later uniform updates
    assetData = asset;

    uboData.baseColorFactor = asset.baseColorFactor;
    uboData.roughnessFactor = asset.roughnessFactor;
    uboData.metallicFactor = asset.metallicFactor;
    uboData.normalScale = asset.normalScale;
    uboData.emissiveStrength = asset.emissiveStrength;
    uboData.blendMode = static_cast<uint32_t>(asset.blendMode);
    uboData.shadingModel = static_cast<uint32_t>(asset.shadingModel);
    uboData.padding = 0;

    // --------------------------------------------------------------
    // Load shader modules (fail fast)
    std::string actualFragPath = fragPath;
    if (asset.blendMode == MaterialBlendMode::Blend) {
        actualFragPath = "shaders/transparent_frag.spv";
    }

    if (!shader.load(vertPath, actualFragPath, res.device)) {
        LOG_ERROR("Material::createPBR – shader load failed.");
        return false;
    }

    // --------------------------------------------------------------
    std::vector<DescriptorInfo> refl = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_SHADER_STAGE_FRAGMENT_BIT},
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_SHADER_STAGE_FRAGMENT_BIT},
        {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_SHADER_STAGE_FRAGMENT_BIT},
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_SHADER_STAGE_FRAGMENT_BIT}
    };
    shader.getDescriptorInfos() = std::move(refl);
    
    // --------------------------------------------------------------
    // 1️⃣ Create pipeline
    if (!createPipeline(res)) return false;

    // --------------------------------------------------------------
    // 2️⃣ Create per‑material uniform buffer
    VkDeviceSize ubSize = sizeof(MaterialGPU);
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = ubSize;
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkResult r = vmaCreateBuffer(res.allocator,
                                 &bufInfo,
                                 &allocInfo,
                                 &uboBuffer,
                                 &uboAllocation, nullptr);
    VK_CHECK(r);
    LOG_INFO("Material PBR uniform buffer created.");
    updateUniform(res);

    // --------------------------------------------------------------
    // 3️⃣ Load textures with color space rules (Task 2.5):
    // - Albedo: sRGB (TextureUsage::Albedo maps to VK_FORMAT_R8G8B8A8_SRGB internally)
    // - Emissive: sRGB (TextureUsage::Emissive maps to VK_FORMAT_R8G8B8A8_SRGB internally)
    // - Normal: linear UNORM (TextureUsage::Normal maps to VK_FORMAT_R8G8B8A8_UNORM internally)
    // - Metallic-Roughness: linear UNORM (TextureUsage::MetallicRoughness maps to VK_FORMAT_R8G8B8A8_UNORM internally)
    // - AO: linear UNORM (TextureUsage::AO maps to VK_FORMAT_R8G8B8A8_UNORM internally)
    
    if (!albedoPath.empty()) {
        albedoTexture = std::make_shared<Texture>();
        if (albedoTexture->loadFromFile(albedoPath, res, TextureUsage::Albedo)) {
            uboData.hasAlbedoMap = 1.0f;
        } else {
            LOG_WARN("Failed to load albedo texture: " + albedoPath + " - using fallback white.");
            albedoTexture.reset(); 
            uboData.hasAlbedoMap = 0.0f;
        }
    } else {
        uboData.hasAlbedoMap = albedoTexture ? 1.0f : 0.0f;
    }

    if (!normalPath.empty()) {
        normalTexture = std::make_shared<Texture>();
        if (normalTexture->loadFromFile(normalPath, res, TextureUsage::Normal)) {
            uboData.useNormalMap = 1.0f;
        } else {
            LOG_WARN("Failed to load normal texture: " + normalPath + " - using fallback flat normal.");
            normalTexture.reset();
            uboData.useNormalMap = 0.0f;
        }
    } else {
        uboData.useNormalMap = normalTexture ? 1.0f : 0.0f;
    }

    if (!metallicRoughnessPath.empty()) {
        metallicRoughnessTexture = std::make_shared<Texture>();
        if (metallicRoughnessTexture->loadFromFile(metallicRoughnessPath, res, TextureUsage::MetallicRoughness)) {
            uboData.hasMetallicRoughnessMap = 1.0f;
        } else {
            LOG_WARN("Failed to load metallic-roughness texture: " + metallicRoughnessPath + " - using fallback metallic=0, roughness=0.6.");
            metallicRoughnessTexture.reset();
            uboData.hasMetallicRoughnessMap = 0.0f;
        }
    } else {
        uboData.hasMetallicRoughnessMap = metallicRoughnessTexture ? 1.0f : 0.0f;
    }

    if (!aoPath.empty()) {
        aoTexture = std::make_shared<Texture>();
        if (aoTexture->loadFromFile(aoPath, res, TextureUsage::AO)) {
            uboData.hasAOMap = 1.0f;
        } else {
            LOG_WARN("Failed to load AO texture: " + aoPath + " - using fallback white.");
            aoTexture.reset();
            uboData.hasAOMap = 0.0f;
        }
    } else {
        uboData.hasAOMap = aoTexture ? 1.0f : 0.0f;
    }

    if (!emissivePath.empty()) {
        emissiveTexture = std::make_shared<Texture>();
        if (emissiveTexture->loadFromFile(emissivePath, res, TextureUsage::Emissive)) {
            uboData.hasEmissiveMap = 1.0f;
        } else {
            LOG_WARN("Failed to load emissive texture: " + emissivePath + " - using fallback black.");
            emissiveTexture.reset();
            uboData.hasEmissiveMap = 0.0f;
        }
    } else {
        uboData.hasEmissiveMap = emissiveTexture ? 1.0f : 0.0f;
    }

    updateUniform(res);

    // --------------------------------------------------------------
    // 4️⃣ Allocate descriptor set
    if (!allocateDescriptorSet(res)) return false;

    // Task 2.3 — Add material validation logs
    {
        std::string nameString = albedoPath;
        if (nameString.empty()) nameString = "unnamed_or_embedded";
        ::Logger::Log(::LogLevel::Info, "[MaterialUpload] Material upload validation details:");
        ::Logger::Log(::LogLevel::Info, "[MaterialUpload]   - Name/Path: " + nameString);
        ::Logger::Log(::LogLevel::Info, "[MaterialUpload]   - Base Color Factor: (" +
            std::to_string(uboData.baseColorFactor.r) + ", " +
            std::to_string(uboData.baseColorFactor.g) + ", " +
            std::to_string(uboData.baseColorFactor.b) + ", " +
            std::to_string(uboData.baseColorFactor.a) + ")");
        ::Logger::Log(::LogLevel::Info, "[MaterialUpload]   - Metallic Factor: " + std::to_string(uboData.metallicFactor));
        ::Logger::Log(::LogLevel::Info, "[MaterialUpload]   - Roughness Factor: " + std::to_string(uboData.roughnessFactor));
        ::Logger::Log(::LogLevel::Info, std::string("[MaterialUpload]   - Albedo map valid: ") + (uboData.hasAlbedoMap > 0.0f ? "YES" : "NO (using fallback white)"));
        ::Logger::Log(::LogLevel::Info, std::string("[MaterialUpload]   - Normal map valid: ") + (uboData.useNormalMap > 0.0f ? "YES" : "NO (using fallback flat normal)"));
        ::Logger::Log(::LogLevel::Info, std::string("[MaterialUpload]   - Metallic-Roughness map valid: ") + (uboData.hasMetallicRoughnessMap > 0.0f ? "YES" : "NO (using fallback metallic=0, roughness=0.6)"));
        ::Logger::Log(::LogLevel::Info, std::string("[MaterialUpload]   - AO map valid: ") + (uboData.hasAOMap > 0.0f ? "YES" : "NO (using fallback white)"));
        ::Logger::Log(::LogLevel::Info, std::string("[MaterialUpload]   - Emissive map valid: ") + (uboData.hasEmissiveMap > 0.0f ? "YES" : "NO (using fallback black)"));

        // Suspicious check: e.g. wood, plastic, fabric, brick, stone should generally not be metallic
        std::string lowerPath = nameString;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        if (uboData.metallicFactor > 0.8f) {
            if (lowerPath.find("wood") != std::string::npos ||
                lowerPath.find("plastic") != std::string::npos ||
                lowerPath.find("fabric") != std::string::npos ||
                lowerPath.find("brick") != std::string::npos ||
                lowerPath.find("stone") != std::string::npos ||
                lowerPath.find("floor") != std::string::npos ||
                lowerPath.find("ground") != std::string::npos ||
                lowerPath.find("matte") != std::string::npos) {
                ::Logger::Log(::LogLevel::Warn, "Warning: Material \"" + nameString + "\" has metallicFactor > 0.8. Non-metallic-like assets should usually be non-metallic.");
            }
        }
    }

    return true;
}

bool Material::createPipeline(const EngineResources& resources)
{
    VkPipelineShaderStageCreateInfo stages[2]{};

    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = shader.vertModule();
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = shader.fragModule();
    stages[1].pName  = "main";

    VkVertexInputBindingDescription bindingDescription = PbrVertex::GetBindingDescription();
    auto attributeDescriptions = PbrVertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAsm{};
    inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAsm.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable        = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    if (resources.debugConfig && resources.debugConfig->disableBackfaceCulling) {
        raster.cullMode = VK_CULL_MODE_NONE;
    } else {
        raster.cullMode = RasterConvention::CullMode;
    }
    raster.frontFace = RasterConvention::FrontFace;
    raster.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = (assetData.blendMode == MaterialBlendMode::Blend) ? VK_FALSE : VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    std::array<VkPipelineColorBlendAttachmentState, 4> blendAttachments{};
    uint32_t attachmentCount = 4;

    if (assetData.blendMode == MaterialBlendMode::Blend) {
        attachmentCount = 1;
        blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                             VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT |
                                             VK_COLOR_COMPONENT_A_BIT;
        blendAttachments[0].blendEnable = VK_TRUE;
        blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        attachmentCount = 4;
        for (uint32_t i = 0; i < 4; ++i) {
            blendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                 VK_COLOR_COMPONENT_G_BIT |
                                                 VK_COLOR_COMPONENT_B_BIT |
                                                 VK_COLOR_COMPONENT_A_BIT;
            blendAttachments[i].blendEnable = VK_FALSE;
        }
    }

    VkPipelineColorBlendStateCreateInfo colourBlendInfo{};
    colourBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colourBlendInfo.logicOpEnable = VK_FALSE;
    colourBlendInfo.attachmentCount = attachmentCount;
    colourBlendInfo.pAttachments = blendAttachments.data();

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = 2;
    dynamicInfo.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAsm;
    pipelineInfo.pViewportState      = &viewportInfo;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState   = &multisample;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colourBlendInfo;
    pipelineInfo.pDynamicState       = &dynamicInfo;
    pipelineInfo.layout              = resources.pipelineLayout;   // includes set 0 + set 1
    VkRenderPass opaquePass = resources.gbufferRenderPass != VK_NULL_HANDLE ? resources.gbufferRenderPass : resources.renderPass;
    pipelineInfo.renderPass          = (assetData.blendMode == MaterialBlendMode::Blend) ? resources.transparentRenderPass : opaquePass;
    pipelineInfo.subpass             = 0;

    VkResult result = vkCreateGraphicsPipelines(resources.device,
                                                VK_NULL_HANDLE,
                                                1,
                                                &pipelineInfo,
                                                nullptr,
                                                &pipelineHandle);
    VK_CHECK(result);
    return true;
}

bool Material::allocateDescriptorSet(const EngineResources& resources)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = resources.materialDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &resources.materialSetLayout;

    VkResult r = vkAllocateDescriptorSets(resources.device, &allocInfo,
                                          &descriptorSet);
    VK_CHECK(r);

    std::array<VkWriteDescriptorSet, 6> writes{};

    VkDescriptorBufferInfo ubInfo{};
    ubInfo.buffer = uboBuffer;
    ubInfo.offset = 0;
    ubInfo.range  = sizeof(MaterialGPU);

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0;          // binding 0 in material layout
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &ubInfo;

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (albedoTexture) {
        albedoInfo.imageView   = albedoTexture->view();
        albedoInfo.sampler     = albedoTexture->sampler();
    } else {
        Texture* white = Texture::getWhiteTexture(resources);
        albedoInfo.imageView   = white->view();
        albedoInfo.sampler     = white->sampler();
    }

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1;   // binding 1 in material layout
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &albedoInfo;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (normalTexture) {
        normalInfo.imageView   = normalTexture->view();
        normalInfo.sampler     = normalTexture->sampler();
    } else {
        Texture* flatNormal = Texture::getFlatNormalTexture(resources);
        normalInfo.imageView   = flatNormal->view();
        normalInfo.sampler     = flatNormal->sampler();
    }

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = descriptorSet;
    writes[2].dstBinding = 2;   // binding 2 in material layout
    writes[2].dstArrayElement = 0;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &normalInfo;

    VkDescriptorImageInfo metallicRoughnessInfo{};
    metallicRoughnessInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (metallicRoughnessTexture) {
        metallicRoughnessInfo.imageView   = metallicRoughnessTexture->view();
        metallicRoughnessInfo.sampler     = metallicRoughnessTexture->sampler();
    } else {
        Texture* mrFallback = Texture::getMetallicRoughnessFallbackTexture(resources);
        metallicRoughnessInfo.imageView   = mrFallback->view();
        metallicRoughnessInfo.sampler     = mrFallback->sampler();
    }

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = descriptorSet;
    writes[3].dstBinding = 3;   // binding 3 in material layout
    writes[3].dstArrayElement = 0;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].pImageInfo = &metallicRoughnessInfo;

    VkDescriptorImageInfo aoInfo{};
    aoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (aoTexture) {
        aoInfo.imageView   = aoTexture->view();
        aoInfo.sampler     = aoTexture->sampler();
    } else {
        Texture* white = Texture::getWhiteTexture(resources);
        aoInfo.imageView   = white->view();
        aoInfo.sampler     = white->sampler();
    }

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = descriptorSet;
    writes[4].dstBinding = 4;   // binding 4 in material layout
    writes[4].dstArrayElement = 0;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].pImageInfo = &aoInfo;

    VkDescriptorImageInfo emissiveInfo{};
    emissiveInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (emissiveTexture) {
        emissiveInfo.imageView   = emissiveTexture->view();
        emissiveInfo.sampler     = emissiveTexture->sampler();
    } else {
        Texture* black = Texture::getBlackTexture(resources);
        emissiveInfo.imageView   = black->view();
        emissiveInfo.sampler     = black->sampler();
    }

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = descriptorSet;
    writes[5].dstBinding = 5;   // binding 5 in material layout
    writes[5].dstArrayElement = 0;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].pImageInfo = &emissiveInfo;

    vkUpdateDescriptorSets(resources.device,
                           static_cast<uint32_t>(writes.size()),
                           writes.data(),
                           0, nullptr);
    return true;
}

void Material::updateUniform(const EngineResources& resources)
{
    void* dst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator,
                           uboAllocation,
                           &dst));
    std::memcpy(dst, &uboData, sizeof(uboData));
    vmaUnmapMemory(resources.allocator, uboAllocation);
}

void Material::destroy()
{
    if (pipelineHandle != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources->device, pipelineHandle, nullptr);
        pipelineHandle = VK_NULL_HANDLE;
    }
    if (uboBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources->allocator, uboBuffer, uboAllocation);
        uboBuffer = VK_NULL_HANDLE;
        uboAllocation = VK_NULL_HANDLE;
    }
    descriptorSet = VK_NULL_HANDLE;

    albedoTexture.reset();
    normalTexture.reset();
    metallicRoughnessTexture.reset();
    aoTexture.reset();
    emissiveTexture.reset();

    shader.destroy();
}

void Material::updateNormalMapCompatibility(const Mesh& mesh) {
    bool normalTextureValid = (normalTexture != nullptr);
    bool canUseNormalMap =
        mesh.hasNormals &&
        mesh.hasUVs &&
        mesh.hasTangents &&
        normalTextureValid;

    float expectedVal = canUseNormalMap ? 1.0f : 0.0f;
    if (uboData.useNormalMap != expectedVal) {
        uboData.useNormalMap = expectedVal;
        if (resources) {
            updateUniform(*resources);
        }
    }
}

} // namespace eng::renderer
