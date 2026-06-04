#include "Core/pch.h"
#include "Material.h"
#include "Core/types/Vertex.h"
#include <cstddef>
#include <cstring>

namespace eng::renderer {

bool Material::create(const std::string& vertPath,
                     const std::string& fragPath,
                     const std::string& albedoPath,
                     const std::string& normalPath,
                     EngineResources& res)
{
    resources = &res;   // store for later uniform updates

    // --------------------------------------------------------------
    // Load shader modules (fail fast)
    if (!shader.load(vertPath, fragPath, res.device)) {
        LOG_ERROR("Material::create – shader load failed.");
        return false;
    }

    // --------------------------------------------------------------
    std::vector<DescriptorInfo> refl = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
    };
    shader.getDescriptorInfos() = std::move(refl);
    
    // --------------------------------------------------------------
    // 1️⃣ Create pipeline
    if (!createPipeline(res)) return false;

    // --------------------------------------------------------------
    // 2️⃣ Create per‑material uniform buffer
    VkDeviceSize ubSize = sizeof(MaterialUBO);
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
    LOG_INFO("Material uniform buffer created.");
    updateUniform(res);

    // --------------------------------------------------------------
    // 3️⃣ Load textures (only if not already provided)
    if (!albedoTexture) {
        albedoTexture = std::make_shared<Texture>();
        std::string path = albedoPath.empty() ? "textures/brick_albedo.png" : albedoPath;
        if (!albedoTexture->loadFromFile(path, res.device, res.allocator, res.commandPools[0], res.graphicsQueue)) {
            LOG_WARN("Failed to load albedo texture: " + path + " - using fallback white.");
            albedoTexture.reset(); 
        }
    }

    if (!normalTexture && !normalPath.empty()) {
        normalTexture = std::make_shared<Texture>();
        if (!normalTexture->loadFromFile(normalPath, res.device, res.allocator, res.commandPools[0], res.graphicsQueue)) {
            LOG_WARN("Failed to load normal texture: " + normalPath + " - using fallback white.");
            normalTexture.reset();
        }
    }

    // --------------------------------------------------------------
    // 4️⃣ Allocate descriptor set
    if (!allocateDescriptorSet(res)) return false;

    LOG_INFO("Material created successfully.");
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

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(PbrVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(PbrVertex, pos);
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(PbrVertex, normal);
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(PbrVertex, uv);

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
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colourBlendAttachment{};
    colourBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                            VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT |
                                            VK_COLOR_COMPONENT_A_BIT;
    colourBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colourBlendInfo{};
    colourBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colourBlendInfo.logicOpEnable = VK_FALSE;
    colourBlendInfo.attachmentCount = 1;
    colourBlendInfo.pAttachments = &colourBlendAttachment;

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
    pipelineInfo.renderPass          = resources.renderPass;
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

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(3);

    VkDescriptorBufferInfo ubInfo{};
    ubInfo.buffer = uboBuffer;
    ubInfo.offset = 0;
    ubInfo.range  = sizeof(MaterialUBO);

    VkWriteDescriptorSet ubWrite{};
    ubWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubWrite.dstSet = descriptorSet;
    ubWrite.dstBinding = 0;          // binding 0 in material layout
    ubWrite.dstArrayElement = 0;
    ubWrite.descriptorCount = 1;
    ubWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubWrite.pBufferInfo = &ubInfo;
    writes.push_back(ubWrite);

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

    VkWriteDescriptorSet albedoWrite{};
    albedoWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    albedoWrite.dstSet = descriptorSet;
    albedoWrite.dstBinding = 1;   // binding 1 in material layout
    albedoWrite.dstArrayElement = 0;
    albedoWrite.descriptorCount = 1;
    albedoWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    albedoWrite.pImageInfo = &albedoInfo;
    writes.push_back(albedoWrite);

    VkDescriptorImageInfo normalInfo{};
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (normalTexture) {
        normalInfo.imageView   = normalTexture->view();
        normalInfo.sampler     = normalTexture->sampler();
    } else {
        Texture* white = Texture::getWhiteTexture(resources);
        normalInfo.imageView   = white->view();
        normalInfo.sampler     = white->sampler();
    }

    VkWriteDescriptorSet normalWrite{};
    normalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    normalWrite.dstSet = descriptorSet;
    normalWrite.dstBinding = 2;   // binding 2 in material layout
    normalWrite.dstArrayElement = 0;
    normalWrite.descriptorCount = 1;
    normalWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalWrite.pImageInfo = &normalInfo;
    writes.push_back(normalWrite);

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

    shader.destroy();
}

} // namespace eng::renderer
