#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include "RenderingEngine/Core/types/GPUSceneBindings.h"

namespace eng::renderer {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 uv;
    };

    struct PbrVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 tangent;

        static inline VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(PbrVertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        static inline std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 4> attributes{};

            attributes[0] = {
                0,
                0,
                VK_FORMAT_R32G32B32_SFLOAT,
                offsetof(PbrVertex, position)
            };

            attributes[1] = {
                1,
                0,
                VK_FORMAT_R32G32B32_SFLOAT,
                offsetof(PbrVertex, normal)
            };

            attributes[2] = {
                2,
                0,
                VK_FORMAT_R32G32_SFLOAT,
                offsetof(PbrVertex, uv)
            };

            attributes[3] = {
                3,
                0,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                offsetof(PbrVertex, tangent)
            };

            return attributes;
        }
    };

    static_assert(sizeof(PbrVertex) == 48, "PbrVertex size must be 48 bytes");
    static_assert(offsetof(PbrVertex, position) == 0, "PbrVertex position offset must be 0");
    static_assert(offsetof(PbrVertex, normal) == 12, "PbrVertex normal offset must be 12");
    static_assert(offsetof(PbrVertex, uv) == 24, "PbrVertex uv offset must be 24");
    static_assert(offsetof(PbrVertex, tangent) == 32, "PbrVertex tangent offset must be 32");

} // namespace eng::renderer
