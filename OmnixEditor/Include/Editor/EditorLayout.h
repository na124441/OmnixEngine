#pragma once
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    enum class WorkspaceProfile {
        Default,
        LevelDesign,
        ECSDebug,
        AssetPackage
    };

    class EditorLayout {
    public:
        /**
         * @brief Build the default Unreal-like symmetrical dockspace layout.
         * @param dockspaceId The active dockspace node ID.
         */
        static void BuildDefaultDockspace(ImGuiID dockspaceId);

        /**
         * @brief Build workspace layout based on selected profile.
         * @param dockspaceId The active dockspace node ID.
         * @param profile Active workspace profile.
         */
        static void BuildWorkspaceLayout(ImGuiID dockspaceId, WorkspaceProfile profile);
    };

} // namespace eng::runtime
