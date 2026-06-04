#pragma once

namespace eng::runtime {

    class EditorDirtyState {
    public:
        void MarkSceneDirty();
        void ClearSceneDirty();

        bool IsSceneDirty() const;

    private:
        bool m_SceneDirty = false;
    };

} // namespace eng::runtime
