#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <type_traits>

namespace eng::renderer {

// 6.1 Instance Allocator Handle
struct GPUSceneInstanceHandle {
    uint32_t index = 0xFFFFFFFF;
    uint32_t generation = 0;

    constexpr GPUSceneInstanceHandle() noexcept = default;
    constexpr GPUSceneInstanceHandle(uint32_t idx, uint32_t gen) noexcept : index(idx), generation(gen) {}

    constexpr bool IsValid() const noexcept {
        return index != 0xFFFFFFFF;
    }

    constexpr bool operator==(const GPUSceneInstanceHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    constexpr bool operator!=(const GPUSceneInstanceHandle& other) const noexcept {
        return !(*this == other);
    }
};

// 6.2 GPU Instance Record Flags & Masks
enum GPUInstanceFlags : uint32_t {
    GPUInstanceFlags_None = 0,
    GPUInstanceFlags_Visible = 1 << 0,
    GPUInstanceFlags_CastShadow = 1 << 1,
    GPUInstanceFlags_VirtualGeometry = 1 << 2,
    GPUInstanceFlags_NegativeScale = 1 << 3,
    GPUInstanceFlags_RenderLayerShift = 8,
    GPUInstanceFlags_RenderLayerMask = 0xFF << GPUInstanceFlags_RenderLayerShift
};

// 6.2 GPU Instance Record
struct GPUGeometryInstance {
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 previousModel = glm::mat4(1.0f);
    glm::vec4 worldBoundsSphere = glm::vec4(0.0f); // xyz = center, w = radius
    uint32_t geometryID = 0;
    uint32_t materialTableOffset = 0;
    uint32_t objectID = 0;
    uint32_t flags = GPUInstanceFlags_Visible | GPUInstanceFlags_CastShadow;
};

// Confirm CPU/GPU alignment & size static assertions
static_assert(sizeof(GPUGeometryInstance) == 160, "GPUGeometryInstance size must be exactly 160 bytes.");
static_assert(offsetof(GPUGeometryInstance, model) == 0, "model must be at offset 0.");
static_assert(offsetof(GPUGeometryInstance, previousModel) == 64, "previousModel must be at offset 64.");
static_assert(offsetof(GPUGeometryInstance, worldBoundsSphere) == 128, "worldBoundsSphere must be at offset 128.");
static_assert(offsetof(GPUGeometryInstance, geometryID) == 144, "geometryID must be at offset 144.");
static_assert(offsetof(GPUGeometryInstance, materialTableOffset) == 148, "materialTableOffset must be at offset 148.");
static_assert(offsetof(GPUGeometryInstance, objectID) == 152, "objectID must be at offset 152.");
static_assert(offsetof(GPUGeometryInstance, flags) == 156, "flags must be at offset 156.");

// Keep GPUInstance as alias for compatibility in other passes/files
using GPUInstance = GPUGeometryInstance;

} // namespace eng::renderer

namespace std {
    template <>
    struct hash<eng::renderer::GPUSceneInstanceHandle> {
        std::size_t operator()(const eng::renderer::GPUSceneInstanceHandle& handle) const noexcept {
            return std::hash<uint64_t>{}((static_cast<uint64_t>(handle.index) << 32) | handle.generation);
        }
    };
}
