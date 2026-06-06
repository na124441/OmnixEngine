#pragma once

#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "ECS/ECSComponents.h"

namespace eng::runtime {

    class TransformWidget {
    public:
        static bool Draw(TransformComponent& transform, EditorDirtyState& dirtyState, bool& outCommitted);
    };

} // namespace eng::runtime
