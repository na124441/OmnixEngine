#include "Editor/EditorLayout.h"
#include "ThirdParty/imgui/imgui_internal.h"

namespace eng::runtime {

    void EditorLayout::BuildDefaultDockspace(ImGuiID dockspaceId) {
        ImGui::DockBuilderRemoveNode(dockspaceId); // Clear out previous configuration
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

        ImGuiID dock_main_id = dockspaceId;
        
        // Split Left (Hierarchy) - 20%
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
        
        // Split Right (Inspector) - 25% (of what's left, i.e., 25% of the remaining 80% is 20% of total)
        ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        
        // Split Bottom (Asset Browser, Console, Diagnostics) - 25%
        ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

        // Dock the windows
        ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
        ImGui::DockBuilderDockWindow("Asset Browser", dock_id_bottom);
        ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
        ImGui::DockBuilderDockWindow("Play Mode Diagnostics", dock_id_bottom);
        ImGui::DockBuilderDockWindow("Viewport", dock_main_id);

        ImGui::DockBuilderFinish(dockspaceId);
    }

} // namespace eng::runtime
