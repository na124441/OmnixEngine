#pragma once
#include "Runtime/world/World.h"
#include "Runtime/render_scene/RenderObject.h"
#include "Runtime/render_scene/RenderView.h"
#include "Runtime/render_scene/MaterialInstance.h"
#include "Runtime/render_scene/Environment.h"
#include "RenderingEngine/Core/memory/LinearAllocator.h"
#include "Core/containers/Array.h"
#include "Core/types/Result.h"

namespace eng::runtime {

    class AssetCache; 
    
    struct RenderScene {
        eng::core::Array<RenderObject> objects;
        eng::core::Array<RenderView> views;
        Environment environment;
    };

    struct Metrics {
        // Placeholder
    };

    class SceneBuilder {
    public:
        explicit SceneBuilder(AssetCache* assetCache);
        ~SceneBuilder() = default;

        template<typename CameraType>
        eng::core::Result Build(const World& world,
            const CameraType& activeCamera,
            eng::core::LinearAllocator<>& frameAllocator,
            RenderScene& outScene) 
        {
            return eng::core::Result();
        }

    private:
        AssetCache* m_AssetCache;
    };

} // namespace eng::runtime
