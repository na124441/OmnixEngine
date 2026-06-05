#pragma once
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    class EditorLayout {
    public:
        /**
         * @brief Build the default Unreal-like symmetrical dockspace layout.
         * @param dockspaceId The active dockspace node ID.
         */
        static void BuildDefaultDockspace(ImGuiID dockspaceId);
    };

} // namespace eng::runtime
