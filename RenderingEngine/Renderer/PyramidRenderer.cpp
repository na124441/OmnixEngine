#include "Core/pch.h"
#include "PyramidRenderer.h"
#include "Core/Vulkan/VkUtils.h"
#include <vulkan/vulkan.h>
#include "Vulkan/VulkanDevice.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <array>
#include <cstring>
#include <cstddef>
#include <cmath>

namespace eng::renderer {

    // Simple math helpers
    static void setIdentity(float* m) {
        memset(m, 0, 16 * sizeof(float));
        m[0] = 1.0f; m[5] = 1.0f; m[10] = 1.0f; m[15] = 1.0f;
    }

    static void setRotateY(float* m, float angle) {
        setIdentity(m);
        float c = cosf(angle);
        float s = sinf(angle);
        m[0] = c; m[2] = -s;
        m[8] = s; m[10] = c;
    }

    static void setLookAt(float* m, float ex, float ey, float ez, float cx, float cy, float cz, float ux, float uy, float uz) {
        float f[3] = {cx - ex, cy - ey, cz - ez};
        float fLen = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
        f[0] /= fLen; f[1] /= fLen; f[2] /= fLen;

        float s[3] = {f[1]*uz - f[2]*uy, f[2]*ux - f[0]*uz, f[0]*uy - f[1]*ux};
        float sLen = sqrtf(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
        s[0] /= sLen; s[1] /= sLen; s[2] /= sLen;

        float u[3] = {s[1]*f[2] - s[2]*f[1], s[2]*f[0] - s[0]*f[2], s[0]*f[1] - s[1]*f[0]};

        setIdentity(m);
        m[0] = s[0]; m[4] = s[1]; m[8]  = s[2];
        m[1] = u[0]; m[5] = u[1]; m[9]  = u[2];
        m[2] =-f[0]; m[6] =-f[1]; m[10] =-f[2];

        m[12] = -(s[0]*ex + s[1]*ey + s[2]*ez);
        m[13] = -(u[0]*ex + u[1]*ey + u[2]*ez);
        m[14] =  (f[0]*ex + f[1]*ey + f[2]*ez);
    }

    static void setPerspective(float* m, float fovY, float aspect, float zNear, float zFar) {
        setIdentity(m);
        float tanHalfFovy = tanf(fovY / 2.0f);
        m[0] = 1.0f / (aspect * tanHalfFovy);
        m[5] = 1.0f / tanHalfFovy;
        m[10] = zFar / (zNear - zFar);
        m[11] = -1.0f;
        m[14] = -(zFar * zNear) / (zFar - zNear);
        m[15] = 0.0f;
        
        m[5] *= -1.0f; // Vulkan inverted Y
    }



    const std::vector<Vertex> vertices = {
        // Base
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        // Top
        {{ 0.0f,  0.5f,  0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f}}
    };

    const std::vector<uint16_t> indices = {
        // Base
        0, 1, 2, 2, 3, 0,
        // Sides
        0, 1, 4,
        1, 2, 4,
        2, 3, 4,
        3, 0, 4
    };



    PyramidRenderer::PyramidRenderer() = default;

    PyramidRenderer::~PyramidRenderer() {
        Shutdown();
    }

    void PyramidRenderer::Initialize(VkInstance instance, eng::vulkan::VulkanDevice* device, VkRenderPass renderPass, VkExtent2D extent) {
        this->instance = instance;
        this->m_DevicePtr = device;
        this->m_Device = device->GetHandle(); // Fixed: Use accessor
        this->m_Extent = extent;

        ::eng::DebugLabel::Init(instance);

        CreateDescriptorSetLayout();
        CreateGraphicsPipeline(renderPass);
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateCommandPools();
        allocateCommandBuffers();
    }

    void PyramidRenderer::Update(float deltaTime, uint32_t currentImage) {
        static float dummyRot = 0.0f;
        dummyRot += 0.016f; // Approximately 60 FPS rotation, entirely decoupled from deltaTime
        m_Rotation = dummyRot;

        UniformBufferObject ubo{};
        setLookAt(ubo.view, 0.0f, 0.0f, 30.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        setPerspective(ubo.proj, 45.0f * (3.14159f / 180.0f), m_Extent.width / (float) m_Extent.height, 0.1f, 1000.0f);

        memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void PyramidRenderer::RecordCommands(VkCommandBuffer commandBuffer, uint32_t currentImage) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) m_Extent.width;
        viewport.height = (float) m_Extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_Extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {m_VertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[currentImage], 0, nullptr);

        // Phase 5: Pass model rotation via push constants
        float model[16];
        setRotateY(model, m_Rotation);
        vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(model), model);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }

    void PyramidRenderer::Shutdown() {
        if (!m_Device) return;
        VkDevice vkDevice = m_Device;

        VK_CHECK(vkDeviceWaitIdle(vkDevice));

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (i < m_UniformBuffers.size() && m_UniformBuffers[i]) {
                vkDestroyBuffer(vkDevice, m_UniformBuffers[i], nullptr);
                vkFreeMemory(vkDevice, m_UniformBuffersMemory[i], nullptr);
                ::eng::ResourceTracker::decBuffer();
            }
        }
        m_UniformBuffers.clear();
        m_UniformBuffersMemory.clear();
        m_UniformBuffersMapped.clear();

        if (m_DescriptorPool) vkDestroyDescriptorPool(vkDevice, m_DescriptorPool, nullptr);
        if (m_DescriptorSetLayout) vkDestroyDescriptorSetLayout(vkDevice, m_DescriptorSetLayout, nullptr);

        if (m_IndexBuffer) {
            vkDestroyBuffer(vkDevice, m_IndexBuffer, nullptr);
            vkFreeMemory(vkDevice, m_IndexBufferMemory, nullptr);
            ::eng::ResourceTracker::decBuffer();
        }

        if (m_VertexBuffer) {
            vkDestroyBuffer(vkDevice, m_VertexBuffer, nullptr);
            vkFreeMemory(vkDevice, m_VertexBufferMemory, nullptr);
            ::eng::ResourceTracker::decBuffer();
        }

        if (m_GraphicsPipeline) vkDestroyPipeline(vkDevice, m_GraphicsPipeline, nullptr);
        if (m_PipelineLayout) vkDestroyPipelineLayout(vkDevice, m_PipelineLayout, nullptr);

        for (auto pool : commandPools) {
            vkDestroyCommandPool(vkDevice, pool, nullptr);
        }

        ::eng::ResourceTracker::validateAtShutdown();

        m_Device = nullptr;
    }

    void PyramidRenderer::CreateCommandPools() {
        commandPools.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_DevicePtr->GetGraphicsQueueFamily();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &commandPools[i]));
        }
    }

    void PyramidRenderer::allocateCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPools[i];
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffers[i]));
        }
    }

    void PyramidRenderer::recordMainPass(VkCommandBuffer cmd, uint32_t currentImage) {
        ::eng::DebugLabel::Begin(cmd, "Main Pass");
        RecordCommands(cmd, currentImage);
        ::eng::DebugLabel::End(cmd);
    }

    void PyramidRenderer::mainLoop(::GLFWwindow* window) {
        while (!glfwWindowShouldClose(window)) {
            this->timer.tick();
            LOG_DEBUG("Frame " + std::to_string(this->frameIndex) + " - dt = " + std::to_string(this->timer.deltaMs()) + " ms");

            glfwPollEvents();
            this->frameIndex = (this->frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        LOG_INFO("=== Frame timing summary ===");
        LOG_INFO("Frames drawn: " + std::to_string(timer.frames()));
        LOG_INFO("Min dt: " + std::to_string(timer.minDeltaMs()) + " ms");
        LOG_INFO("Max dt: " + std::to_string(timer.maxDeltaMs()) + " ms");
    }

    void PyramidRenderer::CreateDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
            LOG_ERROR("Failed to create descriptor set layout!");
        }
    }

    static std::vector<char> ReadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            ENG_LOG_ERROR("Failed to open shader file: {}", filename);
            return {};
        }
        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    VkShaderModule PyramidRenderer::CreateShaderModule(const std::vector<char>& code) {
        if (code.empty()) return VK_NULL_HANDLE;
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VK_CHECK(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule));
        return shaderModule;
    }

    void PyramidRenderer::CreateGraphicsPipeline(VkRenderPass renderPass) {
        auto vertShaderCode = ReadFile("vert.spv");
        auto fragShaderCode = ReadFile("frag.spv");

        VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, uv);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) m_Extent.width;
        viewport.height = (float) m_Extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_Extent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(float) * 16; // mat4

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        VK_CHECK(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline));

        vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
    }

    uint32_t PyramidRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_DevicePtr->GetPhysicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        return 0;
    }

    void PyramidRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        VK_CHECK(vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory));

        VK_CHECK(vkBindBufferMemory(m_Device, buffer, bufferMemory, 0));
        ::eng::ResourceTracker::incBuffer();
    }

    void PyramidRenderer::CreateVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        // In a real engine we'd use a staging buffer. For simplicity, we use host visible memory directly.
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_VertexBuffer, m_VertexBufferMemory);

        void* data;
        VK_CHECK(vkMapMemory(m_Device, m_VertexBufferMemory, 0, bufferSize, 0, &data));
        memcpy(data, vertices.data(), (size_t) bufferSize);
        vkUnmapMemory(m_Device, m_VertexBufferMemory);
    }

    void PyramidRenderer::CreateIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_IndexBuffer, m_IndexBufferMemory);

        void* data;
        VK_CHECK(vkMapMemory(m_Device, m_IndexBufferMemory, 0, bufferSize, 0, &data));
        memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(m_Device, m_IndexBufferMemory);
    }

    void PyramidRenderer::CreateUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_UniformBuffers[i], m_UniformBuffersMemory[i]);
            VK_CHECK(vkMapMemory(m_Device, m_UniformBuffersMemory[i], 0, bufferSize, 0, &m_UniformBuffersMapped[i]));
        }
    }

    void PyramidRenderer::CreateDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VK_CHECK(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool));
    }

    void PyramidRenderer::CreateDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VK_CHECK(vkAllocateDescriptorSets(m_Device, &allocInfo, m_DescriptorSets.data()));

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_UniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_DescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
        }
    }

} // namespace eng::renderer
