#pragma once

namespace eng::renderer {

    class RenderQueue;

    class VisibilitySystem {
    public:
        static void CullAndSort(RenderQueue& queue);
    };

} // namespace eng::renderer
