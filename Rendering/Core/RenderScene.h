#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <string>
#include <glm/glm.hpp>
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/OmnixMeshFormat.h" // For BoundingBox
#include "Rendering/GPUScene/GPUInstance.h"
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
        uint32_t layerMask = 1;
    };

    struct DirectionalLightGPU {
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        float intensity = 0.0f;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float castShadows = 1.0f; // 1.0f for true, 0.0f for false
        glm::mat4 lightSpaceMatrix{1.0f};
        float shadowBias = 0.0015f;
        float shadowSlopeBias = 0.003f;
        float shadowNormalBias = 0.05f;
        float shadowStrength = 1.0f;
        int shadowResolution = 2048;
        int pcfKernelSize = 3;
        float shadowDistance = 75.0f;
        float temperature = 6500.0f;
        uint32_t layerMask = 0xFFFFFFFF;
        glm::mat4 directionalLightProjViews[4];
        glm::vec4 cascadeSplitDepths;
    };

    struct PointLightGPU {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        float radius = 0.0f;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float intensity = 0.0f;
        float temperature = 6500.0f;
        uint32_t layerMask = 0xFFFFFFFF;
        float sourceRadius = 0.0f;
        bool castShadows = true;
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
        float temperature = 6500.0f;
        uint32_t layerMask = 0xFFFFFFFF;
        float sourceRadius = 0.0f;
        bool castShadows = true;
    };

    struct SkyLightData {
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float intensity = 0.0f;
        std::string environmentPath = "";
        float rotation = 0.0f;
        float diffuseIntensity = 1.0f;
        float specularIntensity = 1.0f;
        float exposureOffset = 0.0f;
        int mode = 0; // 0 = Procedural, 1 = HDR Cubemap
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

    using MaterialGPUData = MaterialGPU;

    struct ReflectionProbeData {
        bool enabled = true;
        glm::vec3 position{0.0f};
        glm::vec3 boxMin{-10.0f};
        glm::vec3 boxMax{10.0f};
        float blendDistance = 1.0f;
        float intensity = 1.0f;
        uint32_t priority = 0;
        bool isBox = true;
        std::string capturePath = "";
    };

    struct RenderScene {
        std::vector<RenderMeshInstance> meshInstances;
        std::vector<DirectionalLightGPU> directionalLights;
        std::vector<PointLightGPU> pointLights;
        std::vector<SpotLightGPU> spotLights;
        std::vector<ReflectionProbeData> reflectionProbes;
        CameraData camera;
        SkyLightData skyLight;
        bool sceneHasValidLights = false;
        bool previewLightingActive = false;
    };

} // namespace eng::renderer

