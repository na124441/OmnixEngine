#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <string>
#include <glm/glm.hpp>
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/OmnixMeshFormat.h" // For BoundingBox
#include "RenderingEngine/Renderer/scene/RenderObject.h"
#include "RenderingEngine/Renderer/scene/Mesh.h"
#include "RenderingEngine/Renderer/scene/Material.h"
#include "RenderingEngine/Renderer/scene/ModelLoader.h"
#include "RenderingEngine/Renderer/scene/Scene.h"

namespace eng::renderer {

    struct RenderMeshInstance {
        AssetHandle meshHandle;
        AssetHandle materialHandle;
        glm::mat4 worldMatrix{1.0f};
        glm::mat4 previousWorldMatrix{1.0f};
        BoundingBox worldBounds;
        uint32_t entityID = 0;
        uint32_t flags = 0;
        bool castShadows = true;
    };

    struct DirectionalLightGPU {
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        float intensity = 0.0f;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float castShadows = 1.0f; // 1.0f for true, 0.0f for false
        glm::mat4 lightSpaceMatrix{1.0f};
        float shadowBias = 0.003f;
        float shadowSlopeBias = 0.01f;
        float shadowNormalBias = 0.0f;
        float shadowStrength = 1.0f;
        int shadowResolution = 2048;
        int pcfKernelSize = 3;
    };

    struct PointLightGPU {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        float radius = 0.0f;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float intensity = 0.0f;
    };

    struct SpotLightGPU {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        float range = 0.0f;
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
        float intensity = 0.0f;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float innerConeAngle = 0.0f;
        float outerConeAngle = 0.0f;
        float padding = 0.0f;
    };

    struct SkyLightData {
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float intensity = 0.0f;
    };

    struct CameraData {
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        float fov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        float aspectRatio = 1.777f;
        float exposure = 1.0f;
        uint32_t selectedEntityID = 0;
    };

    struct CameraGPUData {
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
        glm::vec4 cameraPos{0.0f}; // xyz = position, w = fov
        glm::vec4 cameraPlanes{0.1f, 1000.0f, 0.0f, 0.0f}; // x = near, y = far, zw = unused
    };

    struct InstanceGPUData {
        glm::mat4 worldMatrix{1.0f};
        glm::mat4 previousWorldMatrix{1.0f};
        glm::vec4 minBounds_materialIndex{0.0f}; // xyz = min bounds, w = materialIndex
        glm::vec4 maxBounds_entityID{0.0f};      // xyz = max bounds, w = entityID
    };

    using MaterialGPUData = MaterialGPU;

    struct RenderScene {
        std::vector<RenderMeshInstance> meshInstances;
        std::vector<DirectionalLightGPU> directionalLights;
        std::vector<PointLightGPU> pointLights;
        std::vector<SpotLightGPU> spotLights;
        CameraData camera;
        SkyLightData skyLight;
    };

} // namespace eng::renderer

