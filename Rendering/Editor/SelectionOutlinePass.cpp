#include "Core/pch.h"
#include "SelectionOutlinePass.h"

namespace eng::renderer {

void SelectionOutlinePass::Record(VkCommandBuffer cmd) {
    // Stub: No-op for v0.1 (outline outlines are rendered via ImGui DrawList)
}

} // namespace eng::renderer
