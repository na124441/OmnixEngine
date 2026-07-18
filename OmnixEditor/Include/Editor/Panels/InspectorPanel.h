#pragma once

#include "Runtime/RuntimeContext.h"
#include "Editor/EditorSelection.h"
#include "Editor/EditorDirtyState.h"

namespace eng::runtime {

    class InspectorPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render(EditorSelection& selection, EditorDirtyState& dirtyState);

    private:
        RuntimeContext* m_Context = nullptr;
    };

} // namespace eng::runtime
