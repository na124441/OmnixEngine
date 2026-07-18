#pragma once

#include "Core/types/Result.h"

namespace eng::runtime {
    class World;
}

namespace eng::renderer {

    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        [[nodiscard]] virtual eng::core::Result Initialize() = 0;
        virtual void Shutdown() = 0;
        virtual void Tick() = 0;

        virtual void BeginFrame(double deltaTime) = 0;
        virtual void Render() = 0;
        virtual void EndFrame() = 0;

        virtual void SetExternalWorld(eng::runtime::World* world) noexcept = 0;
    };

} // namespace eng::renderer
