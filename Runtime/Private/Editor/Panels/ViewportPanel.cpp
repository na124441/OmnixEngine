#include "Runtime/Private/Editor/Panels/ViewportPanel.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Renderer/SceneRenderer.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace eng::runtime {

    static void DrawGrid(ImDrawList* drawList, const glm::mat4& view, const glm::mat4& proj, ImVec2 imageStartPos, ImVec2 size) {
        int gridRange = 50;
        
        auto projectWorldPoint = [&](const glm::vec3& worldPos, ImVec2& outPos) -> bool {
            glm::vec4 clipPos = proj * view * glm::vec4(worldPos, 1.0f);
            if (clipPos.w <= 0.001f) return false;
            glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
            // Cull points that are too far behind or outside reasonable frustum bounds
            if (std::abs(ndc.x) > 2.0f || std::abs(ndc.y) > 2.0f || ndc.z < 0.0f || ndc.z > 1.0f) return false;
            outPos.x = imageStartPos.x + (ndc.x + 1.0f) * 0.5f * size.x;
            outPos.y = imageStartPos.y + (1.0f - ndc.y) * 0.5f * size.y;
            return true;
        };

        ImU32 minorColor = IM_COL32(80, 80, 80, 70);
        ImU32 majorColor = IM_COL32(120, 120, 120, 150);
        ImU32 xAxisColor = IM_COL32(220, 60, 60, 255); // Red X ground line
        ImU32 zAxisColor = IM_COL32(60, 60, 220, 255); // Blue Z ground line

        for (int i = -gridRange; i <= gridRange; ++i) {
            ImVec2 p1, p2;
            bool isMajor = (i % 5 == 0);
            
            // Lines parallel to Z (constant X)
            ImU32 color = isMajor ? majorColor : minorColor;
            float thickness = isMajor ? 1.5f : 1.0f;
            if (i == 0) {
                color = zAxisColor;
                thickness = 2.0f;
            }
            if (projectWorldPoint(glm::vec3((float)i, 0.0f, -(float)gridRange), p1) &&
                projectWorldPoint(glm::vec3((float)i, 0.0f, (float)gridRange), p2)) {
                drawList->AddLine(p1, p2, color, thickness);
            }

            // Lines parallel to X (constant Z)
            color = isMajor ? majorColor : minorColor;
            thickness = isMajor ? 1.5f : 1.0f;
            if (i == 0) {
                color = xAxisColor;
                thickness = 2.0f;
            }
            if (projectWorldPoint(glm::vec3(-(float)gridRange, 0.0f, (float)i), p1) &&
                projectWorldPoint(glm::vec3((float)gridRange, 0.0f, (float)i), p2)) {
                drawList->AddLine(p1, p2, color, thickness);
            }
        }
    }

    static void DrawAxisGizmo(ImDrawList* drawList, const glm::mat4& view, ImVec2 imageStartPos, ImVec2 size) {
        ImVec2 gizmoCenter = ImVec2(imageStartPos.x + size.x - 60.0f, imageStartPos.y + 60.0f);
        
        glm::mat3 viewRot = glm::mat3(view);
        glm::vec3 screenX = viewRot * glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 screenY = viewRot * glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 screenZ = viewRot * glm::vec3(0.0f, 0.0f, 1.0f);

        float length = 35.0f;
        ImVec2 xEnd(gizmoCenter.x + screenX.x * length, gizmoCenter.y - screenX.y * length);
        ImVec2 yEnd(gizmoCenter.x + screenY.x * length, gizmoCenter.y - screenY.y * length);
        ImVec2 zEnd(gizmoCenter.x + screenZ.x * length, gizmoCenter.y - screenZ.y * length);

        // Draw premium transparent circular background
        drawList->AddCircleFilled(gizmoCenter, 42.0f, IM_COL32(20, 20, 25, 120));
        drawList->AddCircle(gizmoCenter, 42.0f, IM_COL32(80, 80, 90, 80), 0, 1.0f);

        // Draw axes lines (Z, then Y, then X)
        drawList->AddLine(gizmoCenter, xEnd, IM_COL32(240, 70, 70, 255), 2.5f); // Red X
        drawList->AddLine(gizmoCenter, yEnd, IM_COL32(70, 240, 70, 255), 2.5f); // Green Y
        drawList->AddLine(gizmoCenter, zEnd, IM_COL32(70, 70, 240, 255), 2.5f); // Blue Z

        // Draw labels
        drawList->AddText(xEnd, IM_COL32(240, 70, 70, 255), "X");
        drawList->AddText(yEnd, IM_COL32(70, 240, 70, 255), "Y");
        drawList->AddText(zEnd, IM_COL32(70, 70, 240, 255), "Z");
    }

    void ViewportPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
    }

    void ViewportPanel::Render(VkDescriptorSet viewportTexture, float& outWidth, float& outHeight) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport", nullptr);
        ImGui::PopStyleVar();

        m_IsFocused = ImGui::IsWindowFocused();
        m_IsHovered = ImGui::IsWindowHovered();

        // Available Content Size
        ImVec2 size = ImGui::GetContentRegionAvail();
        size.x = std::max(size.x, 1.0f);
        size.y = std::max(size.y, 1.0f);
        
        outWidth = size.x;
        outHeight = size.y;

        ImVec2 imageStartPos = ImGui::GetCursorScreenPos();

        if (viewportTexture != VK_NULL_HANDLE) {
            // Draw offscreen color buffer texture
            ImGui::Image((ImTextureID)viewportTexture, size, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        } else {
            ImGui::Text("3D Scene Viewport");
        }

        // Draw 3D Grid floor and axes
        auto* loop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
        eng::renderer::SceneRenderer* renderer = nullptr;
        if (loop && loop->GetSceneRenderer()) {
            renderer = loop->GetSceneRenderer();
            auto& cam = renderer->getCamera();
            glm::mat4 view = cam.getViewMatrix();
            float aspect = size.x / size.y;
            glm::mat4 proj = cam.getProjMatrix(aspect);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if (m_ShowGrid) {
                DrawGrid(drawList, view, proj, imageStartPos, size);
            }
            DrawAxisGizmo(drawList, view, imageStartPos, size);
        }

        // Viewport Toolbar Overlay (Horizontal floating panel)
        ImGui::SetCursorScreenPos(ImVec2(imageStartPos.x + 10.0f, imageStartPos.y + 10.0f));
        ImGui::BeginChild("ViewportToolbar", ImVec2(size.x - 20.0f, 35.0f), ImGuiChildFlags_None, 
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.1f, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.25f, 0.4f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        
        ImGui::BeginChild("ToolbarFrame", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::AlignTextToFramePadding();
        ImGui::Text(" Mode: "); ImGui::SameLine();
        const char* renderModes[] = { "Lit", "Unlit", "Wireframe" };
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::Combo("##RenderModeCombo", &m_RenderMode, renderModes, IM_ARRAYSIZE(renderModes))) {
            if (renderer) {
                if (m_RenderMode == 1) { // Unlit
                    renderer->lightIntensity = 0.0f;
                    renderer->ambientIntensity = 1.0f;
                    renderer->ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
                } else if (m_RenderMode == 0) { // Lit
                    renderer->lightIntensity = 3.0f;
                    renderer->ambientIntensity = 0.35f;
                    renderer->ambientColor = glm::vec3(0.10f, 0.12f, 0.16f);
                }
                // Wireframe is a placeholder mode as requested
            }
        }
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        ImGui::Checkbox("Grid", &m_ShowGrid);
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        ImGui::Checkbox("Colliders", &m_ShowColliders);
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        
        if (ImGui::Button("Frame Selected")) {
            // Trigger F key press internally by injecting it
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F, true);
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F, false);
        }

        if (renderer) {
            ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
            ImGui::Text("Queue Count: %u", renderer->m_TotalRenderCount);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::EndChild();

        // Viewport Diagnostics overlay
        if (renderer) {
            uint32_t offscreenWidth = renderer->GetOffscreenWidth();
            uint32_t offscreenHeight = renderer->GetOffscreenHeight();
            uint32_t frameIdx = renderer->frameIndex;
            uint32_t renderQueueCount = renderer->m_TotalRenderCount;
            bool textureValid = (viewportTexture != VK_NULL_HANDLE);

            // Draw overlay at bottom-left corner of the viewport panel
            ImGui::SetCursorScreenPos(ImVec2(imageStartPos.x + 10.0f, imageStartPos.y + size.y - 120.0f));
            
            ImGui::BeginChild("ViewportStats", ImVec2(280.0f, 110.0f), ImGuiChildFlags_Borders, 
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.9f, 1.0f));
            ImGui::Text("--- Viewport Diagnostics ---");
            ImGui::Text("Panel Size: %.0fx%.0f", outWidth, outHeight);
            ImGui::Text("Render Target: %ux%u", offscreenWidth, offscreenHeight);
            ImGui::Text("Frame Index: %u (Queue Count: %u)", frameIdx, renderQueueCount);
            ImGui::Text("Texture Handle: %s", textureValid ? "VALID" : "INVALID");
            ImGui::PopStyleColor();
            ImGui::EndChild();
        }

        ImGui::End();
    }

} // namespace eng::runtime
