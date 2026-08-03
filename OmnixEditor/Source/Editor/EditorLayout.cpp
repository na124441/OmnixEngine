#include "Editor/EditorLayout.h"
#include "ThirdParty/imgui/imgui_internal.h"

namespace eng::runtime {

    void EditorLayout::BuildDefaultDockspace(ImGuiID dockspaceId) {
        BuildWorkspaceLayout(dockspaceId, WorkspaceProfile::Default);
    }

    void EditorLayout::BuildWorkspaceLayout(ImGuiID dockspaceId, WorkspaceProfile profile) {
        ImGui::DockBuilderRemoveNode(dockspaceId); // Clear out previous configuration
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

        ImGuiID dock_main_id = dockspaceId;

        if (profile == WorkspaceProfile::LevelDesign) {
            // Focus on large Viewport + Asset Browser + Hierarchy
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.18f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.22f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Asset Browser", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        } else if (profile == WorkspaceProfile::ECSDebug) {
            // Focus on Hierarchy, Inspector, Console & Diagnostics
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.35f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Play Mode Diagnostics", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        } else if (profile == WorkspaceProfile::AssetPackage) {
            // Focus on Asset Browser & Inspector
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.45f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.35f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Asset Browser", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        } else {
            // Default Symmetrical Layout
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Asset Browser", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Play Mode Diagnostics", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        }

        ImGui::DockBuilderFinish(dockspaceId);
    }

} // namespace eng::runtime
