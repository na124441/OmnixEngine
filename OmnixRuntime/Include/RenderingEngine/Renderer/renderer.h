#pragma once
#include "Runtime/render_scene/SceneBuilder.h"

namespace eng::renderer {

    class LegacyRenderer {
    public:
        LegacyRenderer() = default;
        ~LegacyRenderer() = default;
 
        void Render(const eng::runtime::RenderScene& scene) {}
    };

} // namespace eng::renderer
