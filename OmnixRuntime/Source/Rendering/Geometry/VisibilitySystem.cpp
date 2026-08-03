#include "Core/pch.h"
#include "Rendering/Geometry/VisibilitySystem.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"

namespace eng::renderer {

void VisibilitySystem::CullAndSort(RenderQueue& queue) {
    queue.sortByMaterial();
}

} // namespace eng::renderer
