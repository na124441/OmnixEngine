#include "Runtime/Public/Editor/EditorSelection.h"

namespace eng::runtime {

    void EditorSelection::Select(Entity entity) {
        if (entity == 0) {
            Clear();
        } else {
            m_SelectedEntity = entity;
        }
    }

    void EditorSelection::Clear() {
        m_SelectedEntity.reset();
    }

    bool EditorSelection::HasSelection() const {
        return m_SelectedEntity.has_value();
    }

    Entity EditorSelection::GetSelectedEntity() const {
        return m_SelectedEntity.value_or(0);
    }

    void EditorSelection::Validate(Coordinator& coordinator) {
        if (m_SelectedEntity.has_value()) {
            Entity entity = m_SelectedEntity.value();
            if (entity == 0 || !coordinator.IsEntityAlive(entity)) {
                Clear();
            }
        }
    }

} // namespace eng::runtime
