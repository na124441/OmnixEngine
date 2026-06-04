#pragma once

#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Editor/EditorSelection.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"

namespace eng::runtime {

    class InspectorPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render(EditorSelection& selection, EditorDirtyState& dirtyState);

    private:
        RuntimeContext* m_Context = nullptr;
    };

} // namespace eng::runtime
