#include "Editor/Panels/MaterialEditorPanel.h"
#include "ThirdParty/imgui/imgui.h"
#include <fstream>
#include <iostream>

namespace eng::runtime {

    void MaterialEditorPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
    }

    void MaterialEditorPanel::Render() {
        if (!m_IsOpen) return;

        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Material Editor", &m_IsOpen, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }

        char nameBuffer[64];
        snprintf(nameBuffer, sizeof(nameBuffer), "%s", m_Material.name.c_str());
        if (ImGui::InputText("Material Name", nameBuffer, sizeof(nameBuffer))) {
            m_Material.name = nameBuffer;
        }

        ImGui::Separator();
        ImGui::Text("PBR Parameters");

        ImGui::ColorEdit4("Albedo Color", &m_Material.albedoColor[0]);
        ImGui::SliderFloat("Metallic", &m_Material.metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &m_Material.roughness, 0.04f, 1.0f);
        ImGui::SliderFloat("Normal Scale", &m_Material.normalScale, 0.0f, 5.0f);
        ImGui::SliderFloat("Ambient Occlusion", &m_Material.ao, 0.0f, 1.0f);
        ImGui::ColorEdit3("Emissive Color", &m_Material.emissiveColor[0]);

        ImGui::Separator();
        ImGui::Text("Texture Channels (Drag & Drop)");

        auto drawTextureSlot = [](const char* label, std::string& path) {
            ImGui::Text("%s:", label);
            ImGui::SameLine();
            const char* displayStr = path.empty() ? "[ None - Drop Texture Here ]" : path.c_str();
            ImGui::Selectable(displayStr, false, ImGuiSelectableFlags_Disabled, ImVec2(0, 20));
            
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_DROP_ASSET_HANDLE")) {
                    path = "DroppedTexture";
                }
                ImGui::EndDragDropTarget();
            }
        };

        drawTextureSlot("Albedo Map", m_Material.albedoTexturePath);
        drawTextureSlot("Normal Map", m_Material.normalTexturePath);
        drawTextureSlot("Metallic/Roughness", m_Material.metallicRoughnessTexturePath);
        drawTextureSlot("AO Map", m_Material.aoTexturePath);

        ImGui::Separator();
        if (ImGui::Button("Save Material (.omat)", ImVec2(-1, 30))) {
            // Material serialization stub
        }

        ImGui::End();
    }

} // namespace eng::runtime
