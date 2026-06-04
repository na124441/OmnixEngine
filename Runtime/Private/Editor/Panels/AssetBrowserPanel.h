#pragma once

#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Editor/EditorSelection.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "Runtime/Public/AssetHandle.h"
#include <functional>

namespace eng::runtime {

    class AssetBrowserPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render(EditorSelection& selection, EditorDirtyState& dirtyState, std::function<void(AssetHandle)> onCreateEntityFromMesh = nullptr);

        AssetHandle GetSelectedAsset() const { return m_SelectedAsset; }

    private:
        RuntimeContext* m_Context = nullptr;
        AssetHandle m_SelectedAsset;
        int m_FilterType = 0; // 0 = All, 1 = Mesh, 2 = Material, 3 = Texture
    };

} // namespace eng::runtime
