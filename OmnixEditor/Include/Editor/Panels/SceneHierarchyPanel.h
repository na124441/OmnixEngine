#pragma once

#include "Runtime/RuntimeContext.h"
#include "Editor/EditorSelection.h"
#include "Editor/EditorDirtyState.h"

#include <string>

class SceneObject;

namespace eng::runtime {

    class SceneHierarchyPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render(EditorSelection& selection, EditorDirtyState& dirtyState);

    private:
        void DrawNode(::SceneObject* obj, EditorSelection& selection, EditorDirtyState& dirtyState, const std::string& searchFilter);
        void EnsureActiveSceneAndSync();

        RuntimeContext* m_Context = nullptr;
        char m_SearchBuffer[128] = "";
    };

} // namespace eng::runtime
