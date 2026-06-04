#include "Runtime/Public/Editor/EditorDirtyState.h"

namespace eng::runtime {

    void EditorDirtyState::MarkSceneDirty() {
        m_SceneDirty = true;
    }

    void EditorDirtyState::ClearSceneDirty() {
        m_SceneDirty = false;
    }

    bool EditorDirtyState::IsSceneDirty() const {
        return m_SceneDirty;
    }

} // namespace eng::runtime
