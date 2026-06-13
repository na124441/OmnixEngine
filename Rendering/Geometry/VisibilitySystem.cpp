#include "Core/pch.h"
#include "VisibilitySystem.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"

namespace eng::renderer {

void VisibilitySystem::CullAndSort(RenderQueue& queue) {
    queue.sortByMaterial();
}

} // namespace eng::renderer
