#pragma once

#include "ECS/ECSconfig.h"
#include "ECS/Coordinator.h"
#include <optional>

namespace eng::runtime {

    class EditorSelection {
    public:
        void Select(Entity entity);
        void Clear();

        bool HasSelection() const;
        Entity GetSelectedEntity() const;

        void Validate(Coordinator& coordinator);

    private:
        std::optional<Entity> m_SelectedEntity;
    };

} // namespace eng::runtime
