#pragma once
#include "Runtime/render_scene/SceneBuilder.h"

namespace eng::renderer {

    class Renderer {
    public:
        Renderer() = default;
        ~Renderer() = default;

        void Render(const eng::runtime::RenderScene& scene) {}
    };

} // namespace eng::renderer
