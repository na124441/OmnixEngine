#include "Runtime/Private/Editor/Panels/ViewportPanel.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Renderer/SceneRenderer.h"
#include "RenderingEngine/Renderer/scene/Mesh.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/SceneManager.h"
#include "ImGuizmo.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/Editor/EditorSelection.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "Runtime/Public/Editor/EditorMath.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eng::runtime {

    static void DrawGrid(ImDrawList* drawList, const glm::mat4& view, const glm::mat4& proj, ImVec2 imageStartPos, ImVec2 size, const glm::vec3& cameraPos, float gridScale) {
        int gridRange = 50;
        
        auto projectWorldPoint = [&](const glm::vec3& worldPos, ImVec2& outPos, float& outFade) -> bool {
            glm::vec4 clipPos = proj * view * glm::vec4(worldPos, 1.0f);
            if (clipPos.w <= 0.001f) return false;
            glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
            // Cull points that are too far behind or outside reasonable frustum bounds
            if (std::abs(ndc.x) > 2.0f || std::abs(ndc.y) > 2.0f || ndc.z < 0.0f || ndc.z > 1.0f) return false;
            outPos.x = imageStartPos.x + (ndc.x + 1.0f) * 0.5f * size.x;
            outPos.y = imageStartPos.y + (1.0f - ndc.y) * 0.5f * size.y;

            float dist = glm::distance(worldPos, cameraPos);
            float maxFadeDist = 60.0f;
            outFade = 1.0f - glm::clamp(dist / maxFadeDist, 0.0f, 1.0f);
            outFade = outFade * outFade; // smooth quadratic falloff
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
            float fade1 = 0.0f, fade2 = 0.0f;
            if (projectWorldPoint(glm::vec3((float)i * gridScale, 0.0f, -(float)gridRange * gridScale), p1, fade1) &&
                projectWorldPoint(glm::vec3((float)i * gridScale, 0.0f, (float)gridRange * gridScale), p2, fade2)) {
                float fade = (fade1 + fade2) * 0.5f;
                if (fade > 0.01f) {
                    ImU32 alphaAdjustedColor = color;
                    int a = (color >> 24) & 0xFF;
                    a = static_cast<int>(a * fade);
                    alphaAdjustedColor = (color & 0x00FFFFFF) | (a << 24);
                    drawList->AddLine(p1, p2, alphaAdjustedColor, thickness);
                }
            }

            // Lines parallel to X (constant Z)
            color = isMajor ? majorColor : minorColor;
            thickness = isMajor ? 1.5f : 1.0f;
            if (i == 0) {
                color = xAxisColor;
                thickness = 2.0f;
            }
            fade1 = 0.0f;
            fade2 = 0.0f;
            if (projectWorldPoint(glm::vec3(-(float)gridRange * gridScale, 0.0f, (float)i * gridScale), p1, fade1) &&
                projectWorldPoint(glm::vec3((float)gridRange * gridScale, 0.0f, (float)i * gridScale), p2, fade2)) {
                float fade = (fade1 + fade2) * 0.5f;
                if (fade > 0.01f) {
                    ImU32 alphaAdjustedColor = color;
                    int a = (color >> 24) & 0xFF;
                    a = static_cast<int>(a * fade);
                    alphaAdjustedColor = (color & 0x00FFFFFF) | (a << 24);
                    drawList->AddLine(p1, p2, alphaAdjustedColor, thickness);
                }
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

    void ViewportPanel::Render(VkDescriptorSet viewportTexture, float& outWidth, float& outHeight, EditorSelection& selection, EditorDirtyState& dirtyState, EditorSimulationState simulationState, float cameraSpeed) {
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

        // Determine screen-space start of the viewport panel
        ImVec2 imageStartPos = ImGui::GetCursorScreenPos();

        // Store viewport screen position and size for debugging overlays
        m_ViewportScreenX = imageStartPos.x;
        m_ViewportScreenY = imageStartPos.y;
        m_ViewportWidth = size.x;
        m_ViewportHeight = size.y;

        auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
        Scene* activeScene = sceneMgr ? sceneMgr->GetActiveScene() : nullptr;

        if (activeScene == nullptr) {
            // Draw centered empty message
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(imageStartPos, ImVec2(imageStartPos.x + size.x, imageStartPos.y + size.y), IM_COL32(20, 20, 25, 255));
            
            const char* msg = "No Scene Active. Go to File -> New Scene or Open Scene to begin.";
            ImVec2 textSize = ImGui::CalcTextSize(msg);
            ImVec2 textPos = ImVec2(imageStartPos.x + (size.x - textSize.x) * 0.5f, imageStartPos.y + (size.y - textSize.y) * 0.5f);
            drawList->AddText(textPos, IM_COL32(180, 180, 200, 255), msg);
            
            ImGui::End();
            return;
        }
        
        if (viewportTexture != VK_NULL_HANDLE) {
            // Draw offscreen color buffer texture
            ImGui::Image((ImTextureID)viewportTexture, size, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        } else {
            ImGui::Text("3D Scene Viewport");
        }

        // Keyboard hotkeys for changing transform gizmo type (W, E, R)
        if (m_IsFocused && !ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) {
                m_GizmoType = ImGuizmo::TRANSLATE;
            } else if (ImGui::IsKeyPressed(ImGuiKey_E)) {
                m_GizmoType = ImGuizmo::ROTATE;
            } else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                m_GizmoType = ImGuizmo::SCALE;
            }
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
                DrawGrid(drawList, view, proj, imageStartPos, size, cam.position, m_GridScale);
            }
            DrawAxisGizmo(drawList, view, imageStartPos, size);

            // Render 3D Gizmo for selected entity (suppressed while capturing editor camera view)
            Entity selectedEntity = selection.GetSelectedEntity();
            bool disableGizmo = ImGui::IsMouseDown(ImGuiMouseButton_Right);
            if (selectedEntity != 0 && m_Context->ecs && !disableGizmo) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                if (coordinator.IsEntityAlive(selectedEntity) && coordinator.GetSignature(selectedEntity).test(coordinator.GetComponentType<TransformComponent>())) {
                    auto& tc = coordinator.GetComponent<TransformComponent>(selectedEntity);

                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(imageStartPos.x, imageStartPos.y, size.x, size.y);

                    // Build GLM matrix from custom ECS transform components
                    glm::vec3 glmPos(tc.position.x, tc.position.y, tc.position.z);
                    glm::quat glmRot(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);
                    glm::vec3 glmScale(tc.scale.x, tc.scale.y, tc.scale.z);

                    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glmPos) *
                                            glm::mat4_cast(glmRot) *
                                            glm::scale(glm::mat4(1.0f), glmScale);

                    // Manipulate matrix using ImGuizmo
                    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                             (ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL,
                                             glm::value_ptr(modelMatrix))) {
                        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
                        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix),
                                                              matrixTranslation,
                                                              matrixRotation,
                                                              matrixScale);

                        tc.position = Vector3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
                        tc.rotation = EulerToQuaternion(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
                        tc.scale = Vector3(matrixScale[0], matrixScale[1], matrixScale[2]);
                        tc.dirty = true;

                        dirtyState.MarkSceneDirty();
                    }

                    // --- Render 12-edge Selection Bounding Box Outline ---
                    glm::vec3 localMin(-0.5f);
                    glm::vec3 localMax(0.5f);
                    bool hasBounds = false;

                    if (coordinator.GetSignature(selectedEntity).test(coordinator.GetComponentType<RenderableMeshComponent>())) {
                        const auto& rm = coordinator.GetComponent<RenderableMeshComponent>(selectedEntity);
                        if (rm.meshAssetHandle.IsValid() && renderer->m_EcsMeshCache.find(rm.meshAssetHandle.value) != renderer->m_EcsMeshCache.end()) {
                            eng::renderer::Mesh* m = renderer->m_EcsMeshCache[rm.meshAssetHandle.value];
                            if (m) {
                                localMin = m->minBounds;
                                localMax = m->maxBounds;
                                hasBounds = true;
                            }
                        }
                    }

                    if (!hasBounds && coordinator.GetSignature(selectedEntity).test(coordinator.GetComponentType<BoxColliderComponent>())) {
                        const auto& bc = coordinator.GetComponent<BoxColliderComponent>(selectedEntity);
                        glm::vec3 offset(bc.offset.x, bc.offset.y, bc.offset.z);
                        glm::vec3 bsize(bc.size.x, bc.size.y, bc.size.z);
                        localMin = offset - bsize * 0.5f;
                        localMax = offset + bsize * 0.5f;
                        hasBounds = true;
                    }

                    if (!hasBounds && coordinator.GetSignature(selectedEntity).test(coordinator.GetComponentType<SphereColliderComponent>())) {
                        const auto& sc = coordinator.GetComponent<SphereColliderComponent>(selectedEntity);
                        glm::vec3 offset(sc.offset.x, sc.offset.y, sc.offset.z);
                        float r = sc.radius;
                        localMin = offset - glm::vec3(r);
                        localMax = offset + glm::vec3(r);
                        hasBounds = true;
                    }

                    if (!hasBounds && coordinator.GetSignature(selectedEntity).test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
                        const auto& cc = coordinator.GetComponent<CapsuleColliderComponent>(selectedEntity);
                        glm::vec3 offset(cc.offset.x, cc.offset.y, cc.offset.z);
                        float r = cc.radius;
                        float h = cc.height;
                        localMin = offset - glm::vec3(r, h * 0.5f, r);
                        localMax = offset + glm::vec3(r, h * 0.5f, r);
                        hasBounds = true;
                    }

                    // Define local corners
                    glm::vec3 corners[8] = {
                        glm::vec3(localMin.x, localMin.y, localMin.z),
                        glm::vec3(localMax.x, localMin.y, localMin.z),
                        glm::vec3(localMax.x, localMax.y, localMin.z),
                        glm::vec3(localMin.x, localMax.y, localMin.z),
                        glm::vec3(localMin.x, localMin.y, localMax.z),
                        glm::vec3(localMax.x, localMin.y, localMax.z),
                        glm::vec3(localMax.x, localMax.y, localMax.z),
                        glm::vec3(localMin.x, localMax.y, localMax.z)
                    };

                    // Transform corners to world space
                    glm::vec3 worldCorners[8];
                    for (int i = 0; i < 8; ++i) {
                        worldCorners[i] = glm::vec3(modelMatrix * glm::vec4(corners[i], 1.0f));
                    }

                    // Project corners to screen
                    auto projectWorldToScreen = [&](const glm::vec3& worldPos, ImVec2& outPos) -> bool {
                        glm::vec4 clipPos = proj * view * glm::vec4(worldPos, 1.0f);
                        if (clipPos.w <= 0.001f) return false;
                        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                        if (ndc.z < 0.0f || ndc.z > 1.0f) return false;
                        outPos.x = imageStartPos.x + (ndc.x + 1.0f) * 0.5f * size.x;
                        outPos.y = imageStartPos.y + (1.0f - ndc.y) * 0.5f * size.y;
                        return true;
                    };

                    ImVec2 screenCorners[8];
                    bool cornerVisible[8];
                    for (int i = 0; i < 8; ++i) {
                        screenCorners[i] = ImVec2(0.0f, 0.0f);
                        cornerVisible[i] = projectWorldToScreen(worldCorners[i], screenCorners[i]);
                    }

                    // Draw the 12 edges
                    int edges[12][2] = {
                        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Bottom
                        {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Top
                        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Pillars
                    };

                    ImU32 outlineColor = IM_COL32(255, 140, 0, 220); // Orange
                    for (int i = 0; i < 12; ++i) {
                        int p1_idx = edges[i][0];
                        int p2_idx = edges[i][1];
                        if (cornerVisible[p1_idx] && cornerVisible[p2_idx]) {
                            drawList->AddLine(screenCorners[p1_idx], screenCorners[p2_idx], outlineColor, 2.0f);
                        }
                    }
                }
            }
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
        const char* renderModes[] = { "Scene Lights", "Preview Sunny", "Unlit", "Wireframe" };
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::Combo("##RenderModeCombo", &m_RenderMode, renderModes, IM_ARRAYSIZE(renderModes))) {
            if (renderer) {
                if (m_RenderMode == 0) {
                    renderer->m_UseEditorDefaultLighting = false;
                    renderer->m_ShadingMode = 0;
                } else if (m_RenderMode == 1) {
                    renderer->m_UseEditorDefaultLighting = true;
                    renderer->m_ShadingMode = 0;
                } else if (m_RenderMode == 2) {
                    renderer->m_ShadingMode = 1;
                } else if (m_RenderMode == 3) {
                    renderer->m_ShadingMode = 1;
                }
            }
        }
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        ImGui::Checkbox("Grid", &m_ShowGrid);

        if (m_ShowGrid) {
            ImGui::SameLine();
            const char* gridScales[] = { "1m", "5m", "10m" };
            int currentScaleIdx = 0;
            if (m_GridScale == 5.0f) currentScaleIdx = 1;
            else if (m_GridScale == 10.0f) currentScaleIdx = 2;
            
            ImGui::SetNextItemWidth(60.0f);
            if (ImGui::Combo("##GridScaleCombo", &currentScaleIdx, gridScales, IM_ARRAYSIZE(gridScales))) {
                if (currentScaleIdx == 0) m_GridScale = 1.0f;
                else if (currentScaleIdx == 1) m_GridScale = 5.0f;
                else if (currentScaleIdx == 2) m_GridScale = 10.0f;
            }
        }
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        ImGui::Checkbox("Colliders", &m_ShowColliders);

        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        ImGui::Checkbox("Bounds", &m_ShowBounds);
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        
        if (ImGui::Button("Frame Selected")) {
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

        // Info Card Overlay (floating top-left)
        ImGui::SetCursorScreenPos(ImVec2(imageStartPos.x + 10.0f, imageStartPos.y + 50.0f));
        ImGui::BeginChild("ViewportInfoCard", ImVec2(220.0f, 115.0f), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.1f, 0.65f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.25f, 0.3f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        
        ImGui::BeginChild("InfoCardFrame", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        
        std::string sceneName = "None";
        if (activeScene) {
            sceneName = activeScene->GetName();
            if (dirtyState.IsSceneDirty()) {
                sceneName += "*";
            }
        }
        ImGui::Text("Scene: %s", sceneName.c_str());
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        
        Entity selectedEntity = selection.GetSelectedEntity();
        if (selectedEntity != 0) {
            ImGui::Text("Selection: Entity %u", selectedEntity);
        } else {
            ImGui::Text("Selection: None");
        }
        
        ImGui::Text("Cam Speed: %.1f m/s", cameraSpeed);
        
        ImGui::Text("Mode: "); ImGui::SameLine();
        if (simulationState == EditorSimulationState::Edit) {
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Edit");
        } else if (simulationState == EditorSimulationState::Play) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Play");
        } else if (simulationState == EditorSimulationState::Pause) {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.1f, 1.0f), "Pause");
        } else if (simulationState == EditorSimulationState::Step) {
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.1f, 1.0f), "Step");
        }
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::EndChild();

        // Design Warnings (floating top-right)
        bool hasCamera = false;
        bool hasLight = false;
        bool hasPlayerStart = false;

         if (m_Context && m_Context->ecs) {
            auto& coordinator = m_Context->ecs->getCoordinator();
            auto camType = coordinator.GetComponentType<CameraComponent>();
            auto playerStartType = coordinator.GetComponentType<PlayerStartComponent>();
            auto dirLightType = coordinator.GetComponentType<DirectionalLightComponent>();
            auto pointLightType = coordinator.GetComponentType<PointLightComponent>();
            auto ambientLightType = coordinator.GetComponentType<AmbientLightComponent>();
            auto spotLightType = coordinator.GetComponentType<SpotLightComponent>();

            for (Entity ent : coordinator.GetActiveEntities()) {
                if (ent == 0 || !coordinator.IsEntityAlive(ent)) continue;
                const auto& sig = coordinator.GetSignature(ent);
                if (sig.test(camType)) {
                    hasCamera = true;
                }
                if (sig.test(playerStartType)) {
                    hasPlayerStart = true;
                }
                if (sig.test(dirLightType) || sig.test(pointLightType) || sig.test(ambientLightType) || sig.test(spotLightType)) {
                    hasLight = true;
                }
            }
        }

        if (!hasCamera || !hasLight || !hasPlayerStart) {
            float warningBoxHeight = 10.0f;
            if (!hasCamera) warningBoxHeight += 20.0f;
            if (!hasLight) warningBoxHeight += 20.0f;
            if (!hasPlayerStart) warningBoxHeight += 20.0f;

            ImGui::SetCursorScreenPos(ImVec2(imageStartPos.x + size.x - 230.0f, imageStartPos.y + 110.0f));
            ImGui::BeginChild("ViewportWarnings", ImVec2(220.0f, warningBoxHeight), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
            
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.05f, 0.05f, 0.75f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.8f, 0.2f, 0.2f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            
            ImGui::BeginChild("WarningsFrame", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
            
            if (!hasCamera) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: No Camera in scene");
            }
            if (!hasLight) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: No Light in scene");
            }
            if (!hasPlayerStart) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: No PlayerSpawn point");
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::EndChild();
        }

        // Flashing play mode banner at the bottom
        if (simulationState == EditorSimulationState::Play || simulationState == EditorSimulationState::Pause) {
            float time = static_cast<float>(ImGui::GetTime());
            float flash = 0.5f + 0.5f * std::sin(time * 6.0f);
            
            ImVec4 bannerBg = (simulationState == EditorSimulationState::Play) 
                ? ImVec4(0.1f, 0.4f + 0.2f * flash, 0.1f, 0.85f)
                : ImVec4(0.4f + 0.2f * flash, 0.4f, 0.1f, 0.85f);
            
            std::string bannerText = (simulationState == EditorSimulationState::Play)
                ? "PLAYING - Press ESC to release mouse | Shift+F5 Stop | F10 Pause"
                : "PAUSED - Press F10 to Resume | Shift+F5 Stop";

            ImGui::SetCursorScreenPos(ImVec2(imageStartPos.x, imageStartPos.y + size.y - 30.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, bannerBg);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            
            ImGui::BeginChild("PlayBanner", ImVec2(size.x, 30.0f), ImGuiChildFlags_None, 
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            
            ImVec2 textSize = ImGui::CalcTextSize(bannerText.c_str());
            ImGui::SetCursorPos(ImVec2((size.x - textSize.x) * 0.5f, (30.0f - textSize.y) * 0.5f));
            ImGui::Text("%s", bannerText.c_str());
            
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        // Viewport Diagnostics overlay
        if (renderer && m_ShowDiagnostics) {
            uint32_t offscreenWidth = renderer->GetOffscreenWidth();
            uint32_t offscreenHeight = renderer->GetOffscreenHeight();
            uint32_t frameIdx = renderer->frameIndex;
            uint32_t renderQueueCount = renderer->m_TotalRenderCount;
            bool textureValid = (viewportTexture != VK_NULL_HANDLE);

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
