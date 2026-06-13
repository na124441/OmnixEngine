#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include "Rendering/Core/RenderTypes.h"
#include "Rendering/Core/RenderScene.h"
#include "RenderingEngine/Renderer/LightingUBO.h"
#include "RenderingEngine/Renderer/scene/Scene.h"

struct CameraComponent;

namespace eng::runtime {
    class IECSWorld;
    class World;
    class AssetRegistry;
}

namespace eng::renderer {

    class RenderQueue;
    struct EngineResources;

    class RenderSceneExtractor {
    public:
        static void ExtractScene(
            eng::runtime::IECSWorld& world,
            eng::runtime::AssetRegistry* assetRegistry,
            const CameraComponent& activeCamera,
            const glm::mat4& cameraWorldMatrix,
            RenderScene& outRenderScene,
            bool useEditorDefaultLighting = true
        );

        static void DebugPrint(const RenderScene& scene);

        static void ExtractLighting(
            eng::runtime::World* world,
            bool useEditorDefaultLighting,
            uint32_t shadingMode,
            LightData& uboData,
            bool& lastFallbackActive
        );
    };

} // namespace eng::renderer
