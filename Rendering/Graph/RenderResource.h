#pragma once
#include <string>
#include <vulkan/vulkan.h>

namespace eng::renderer {

    enum class RenderResourceType : uint32_t {
        Texture = 0,
        Buffer
    };

    struct TextureResourceDesc {
        uint32_t width = 0;
        uint32_t height = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
    };

    struct BufferResourceDesc {
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
    };

    struct RenderResource {
        std::string name;
        RenderResourceType type = RenderResourceType::Texture;
        bool isTransient = true;

        TextureResourceDesc textureDesc;
        BufferResourceDesc bufferDesc;

        uint32_t firstPass = 0xFFFFFFFF;
        uint32_t lastPass = 0xFFFFFFFF;
    };

} // namespace eng::renderer
