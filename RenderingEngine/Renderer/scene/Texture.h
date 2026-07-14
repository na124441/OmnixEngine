#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "Core/Engine/Log.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/ResourceTracker.h"

namespace eng::renderer {

enum class TextureUsage {
    Albedo,
    Normal,
    MetallicRoughness,
    AO,
    Emissive,
    UI,
    Data
};

class Texture
{
public:
    Texture() = default;
    ~Texture() { destroy(); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept = default;
    Texture& operator=(Texture&&) noexcept = default;

    /// Load an image from disk with stb_image, upload it to a GPU-only
    /// image, create a view and a sampler.
    bool loadFromFile(const std::string& filename,
                      const struct EngineResources& res,
                      TextureUsage usage = TextureUsage::Albedo);

    bool createCubemapFromData(
        const std::vector<std::vector<std::vector<float>>>& mipFacePixels,
        uint32_t width,
        uint32_t height,
        uint32_t mips,
        const struct EngineResources& res);

    bool create2DTextureFromData(
        const std::vector<float>& pixels,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        const struct EngineResources& res);

    VkImageView view()   const { return imageView; }
    VkSampler   sampler() const { return samplerHandle; }

    /** Returns a fallback 1x1 white texture (cached, survives until shutdown). */
    static Texture* getWhiteTexture(const struct EngineResources& res);

    /** Returns a fallback 1x1 flat normal texture (cached, survives until shutdown). */
    static Texture* getFlatNormalTexture(const struct EngineResources& res);

    /** Returns a fallback 1x1 black texture (cached, survives until shutdown). */
    static Texture* getBlackTexture(const struct EngineResources& res);

    /** Returns a fallback 1x1 metallic-roughness texture (metallic=0, roughness=0.6: G=153, B=0) */
    static Texture* getMetallicRoughnessFallbackTexture(const struct EngineResources& res);

    /** Returns a fallback 1x1x6 white cubemap (cached, survives until shutdown). */
    static Texture* getWhiteCubemap(const struct EngineResources& res);

    /** Returns a fallback 2x2 magenta/black checkerboard texture for invalid references */
    static Texture* getErrorCheckerTexture(const struct EngineResources& res);

    static void cleanupFallbackTextures();

    void destroy();

private:
    VkDevice          device = VK_NULL_HANDLE;
    VmaAllocator     allocator = VK_NULL_HANDLE;
    VkImage           image = VK_NULL_HANDLE;
    VmaAllocation    allocation = VK_NULL_HANDLE;
    VkImageView       imageView = VK_NULL_HANDLE;
    VkSampler         samplerHandle = VK_NULL_HANDLE;
    uint32_t          mipLevels = 1;
};

} // namespace eng::renderer
