#include "Runtime/Private/Editor/Panels/ViewportPanel.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "Rendering/Core/Renderer.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "RenderingEngine/Renderer/scene/Mesh.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/SceneManager.h"
#include "ImGuizmo.h"
#include "ECS/ECSComponents.h"
#include "ECS/LightCollectionSystem.h"
#include "ECS/Public/IECSWorld.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/Editor/EditorSelection.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "Runtime/Public/Editor/EditorSceneService.h"
#include "Runtime/Public/Editor/EditorMath.h"
#include "Core/World.h"
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
            } else if (ImGui::IsKeyPressed(ImGuiKey_G)) {
                m_EnableSnapping = !m_EnableSnapping;
            }

            bool hasSelection = selection.HasSelection();
            Entity selectedEntity = selection.GetSelectedEntity();
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                if (hasSelection) {
                    if (m_Context->physicsWorld) {
                        m_Context->physicsWorld->UnregisterEntity(selectedEntity);
                    }
                    auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                    auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                    EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DeleteObject(selectedEntity);
                    selection.Clear();
                }
            }
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D)) {
                if (hasSelection) {
                    auto* sceneMgr = m_Context ? dynamic_cast<SceneManager*>(m_Context->scenes) : nullptr;
                    auto* world = m_Context ? dynamic_cast<World*>(m_Context->ecs) : nullptr;
                    EditorSceneService(sceneMgr, world, &dirtyState, &selection, nullptr).DuplicateObject(selectedEntity);
                }
            }
        }

        // Draw 3D Grid floor and axes
        auto* loop = m_Context ? dynamic_cast<eng::runtime::EngineLoop*>(m_Context->renderer) : nullptr;
        eng::renderer::Renderer* renderer = nullptr;
        if (loop && loop->GetSceneRenderer()) {
            renderer = loop->GetSceneRenderer();
            auto& cam = renderer->getCamera();
            glm::mat4 view = cam.getViewMatrix();
            float aspect = size.x / size.y;
            glm::mat4 proj = cam.getProjMatrix(aspect);

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            auto projectWorldToScreen = [&](const glm::vec3& worldPos, ImVec2& outPos) -> bool {
                glm::vec4 clipPos = proj * view * glm::vec4(worldPos, 1.0f);
                if (clipPos.w <= 0.001f) return false;
                glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
                if (ndc.z < 0.0f || ndc.z > 1.0f) return false;
                outPos.x = imageStartPos.x + (ndc.x + 1.0f) * 0.5f * size.x;
                outPos.y = imageStartPos.y + (1.0f - ndc.y) * 0.5f * size.y;
                return true;
            };

            auto drawWireframeBox = [&](const glm::mat4& modelMatrix, const glm::vec3& localMin, const glm::vec3& localMax, ImU32 color) {
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
                ImVec2 screenCorners[8];
                bool cornerVisible[8];
                for (int i = 0; i < 8; ++i) {
                    glm::vec3 worldCorner = glm::vec3(modelMatrix * glm::vec4(corners[i], 1.0f));
                    screenCorners[i] = ImVec2(0.0f, 0.0f);
                    cornerVisible[i] = projectWorldToScreen(worldCorner, screenCorners[i]);
                }
                int edges[12][2] = {
                    {0, 1}, {1, 2}, {2, 3}, {3, 0},
                    {4, 5}, {5, 6}, {6, 7}, {7, 4},
                    {0, 4}, {1, 5}, {2, 6}, {3, 7}
                };
                for (int i = 0; i < 12; ++i) {
                    int p1_idx = edges[i][0];
                    int p2_idx = edges[i][1];
                    if (cornerVisible[p1_idx] && cornerVisible[p2_idx]) {
                        drawList->AddLine(screenCorners[p1_idx], screenCorners[p2_idx], color, 1.5f);
                    }
                }
            };

            auto drawWireframeCircle = [&](const glm::vec3& center, float radius, const glm::vec3& axis1, const glm::vec3& axis2, ImU32 color, int segments = 32) {
                ImVec2 prevPoint;
                bool prevValid = false;
                for (int i = 0; i <= segments; ++i) {
                    float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
                    glm::vec3 worldPos = center + radius * (std::cos(theta) * axis1 + std::sin(theta) * axis2);
                    ImVec2 screenPos;
                    if (projectWorldToScreen(worldPos, screenPos)) {
                        if (prevValid) {
                            drawList->AddLine(prevPoint, screenPos, color, 1.5f);
                        }
                        prevPoint = screenPos;
                        prevValid = true;
                    } else {
                        prevValid = false;
                    }
                }
            };

            auto drawWireframeSphere = [&](const glm::mat4& modelMatrix, const glm::vec3& localCenter, float radius, ImU32 color) {
                glm::vec3 worldCenter = glm::vec3(modelMatrix * glm::vec4(localCenter, 1.0f));
                float sx = glm::length(glm::vec3(modelMatrix[0]));
                float sy = glm::length(glm::vec3(modelMatrix[1]));
                float sz = glm::length(glm::vec3(modelMatrix[2]));
                float maxScale = std::max({sx, sy, sz});
                float worldRadius = radius * maxScale;

                drawWireframeCircle(worldCenter, worldRadius, glm::vec3(1,0,0), glm::vec3(0,1,0), color);
                drawWireframeCircle(worldCenter, worldRadius, glm::vec3(1,0,0), glm::vec3(0,0,1), color);
                drawWireframeCircle(worldCenter, worldRadius, glm::vec3(0,1,0), glm::vec3(0,0,1), color);
            };

            auto drawWireframeCapsule = [&](const glm::mat4& modelMatrix, const glm::vec3& localCenter, float radius, float height, ImU32 color) {
                float halfH = height * 0.5f - radius;
                if (halfH < 0.0f) halfH = 0.0f;

                glm::vec3 topCenter = localCenter + glm::vec3(0, halfH, 0);
                glm::vec3 bottomCenter = localCenter - glm::vec3(0, halfH, 0);

                auto drawLocalCircleXZ = [&](const glm::vec3& localC, float r) {
                    ImVec2 prevPoint;
                    bool prevValid = false;
                    int segments = 32;
                    for (int i = 0; i <= segments; ++i) {
                        float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
                        glm::vec3 localPos = localC + r * glm::vec3(std::cos(theta), 0.0f, std::sin(theta));
                        glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(localPos, 1.0f));
                        ImVec2 screenPos;
                        if (projectWorldToScreen(worldPos, screenPos)) {
                            if (prevValid) {
                                drawList->AddLine(prevPoint, screenPos, color, 1.5f);
                            }
                            prevPoint = screenPos;
                            prevValid = true;
                        } else {
                            prevValid = false;
                        }
                    }
                };

                drawLocalCircleXZ(topCenter, radius);
                drawLocalCircleXZ(bottomCenter, radius);

                glm::vec3 dirs[4] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
                for (int i = 0; i < 4; ++i) {
                    glm::vec3 localP1 = bottomCenter + dirs[i] * radius;
                    glm::vec3 localP2 = topCenter + dirs[i] * radius;
                    glm::vec3 w1 = glm::vec3(modelMatrix * glm::vec4(localP1, 1.0f));
                    glm::vec3 w2 = glm::vec3(modelMatrix * glm::vec4(localP2, 1.0f));
                    ImVec2 s1, s2;
                    if (projectWorldToScreen(w1, s1) && projectWorldToScreen(w2, s2)) {
                        drawList->AddLine(s1, s2, color, 1.5f);
                    }
                }

                auto drawArc = [&](const glm::vec3& localC, float r, float startAngle, float endAngle, const glm::vec3& axis1, const glm::vec3& axis2) {
                    ImVec2 prevPoint;
                    bool prevValid = false;
                    int segments = 16;
                    for (int i = 0; i <= segments; ++i) {
                        float angle = startAngle + (endAngle - startAngle) * (float)i / (float)segments;
                        glm::vec3 localPos = localC + r * (std::cos(angle) * axis1 + std::sin(angle) * axis2);
                        glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(localPos, 1.0f));
                        ImVec2 screenPos;
                        if (projectWorldToScreen(worldPos, screenPos)) {
                            if (prevValid) {
                                drawList->AddLine(prevPoint, screenPos, color, 1.5f);
                            }
                            prevPoint = screenPos;
                            prevValid = true;
                        } else {
                            prevValid = false;
                        }
                    }
                };

                drawArc(topCenter, radius, 0.0f, glm::pi<float>(), glm::vec3(1,0,0), glm::vec3(0,1,0));
                drawArc(bottomCenter, radius, glm::pi<float>(), 2.0f * glm::pi<float>(), glm::vec3(1,0,0), glm::vec3(0,1,0));
                drawArc(topCenter, radius, 0.0f, glm::pi<float>(), glm::vec3(0,0,1), glm::vec3(0,1,0));
                drawArc(bottomCenter, radius, glm::pi<float>(), 2.0f * glm::pi<float>(), glm::vec3(0,0,1), glm::vec3(0,1,0));
            };

            auto drawWireframeCone = [&](const glm::vec3& apex, const glm::quat& rotation, float range, float outerAngle, ImU32 color) {
                glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);

                glm::vec3 baseCenter = apex + forward * range;
                float baseRadius = range * std::tan(glm::radians(outerAngle));

                drawWireframeCircle(baseCenter, baseRadius, right, up, color);

                glm::vec3 edges[4] = {
                    baseCenter + right * baseRadius,
                    baseCenter - right * baseRadius,
                    baseCenter + up * baseRadius,
                    baseCenter - up * baseRadius
                };

                ImVec2 sApex;
                if (projectWorldToScreen(apex, sApex)) {
                    for (int i = 0; i < 4; ++i) {
                        ImVec2 sEdge;
                        if (projectWorldToScreen(edges[i], sEdge)) {
                            drawList->AddLine(sApex, sEdge, color, 1.5f);
                        }
                    }
                }
            };

            if (m_ShowGrid) {
                DrawGrid(drawList, view, proj, imageStartPos, size, cam.position, m_GridScale);
            }
            Entity selectedEntity = selection.GetSelectedEntity();
            bool disableGizmo = ImGui::IsMouseDown(ImGuiMouseButton_Right);

            if (m_ShowGizmos) {
                DrawAxisGizmo(drawList, view, imageStartPos, size);

                // Render 3D Gizmo for selected entity (suppressed while capturing editor camera view)
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
                        float snapValues[3] = { 0.0f, 0.0f, 0.0f };
                        if (m_EnableSnapping) {
                            if (m_GizmoType == ImGuizmo::TRANSLATE) {
                                snapValues[0] = m_TranslationSnapValue;
                                snapValues[1] = m_TranslationSnapValue;
                                snapValues[2] = m_TranslationSnapValue;
                            } else if (m_GizmoType == ImGuizmo::ROTATE) {
                                snapValues[0] = m_RotationSnapValue;
                                snapValues[1] = m_RotationSnapValue;
                                snapValues[2] = m_RotationSnapValue;
                            } else if (m_GizmoType == ImGuizmo::SCALE) {
                                snapValues[0] = m_ScaleSnapValue;
                                snapValues[1] = m_ScaleSnapValue;
                                snapValues[2] = m_ScaleSnapValue;
                            }
                        }

                        if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                                 (ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL,
                                                 glm::value_ptr(modelMatrix), nullptr, m_EnableSnapping ? snapValues : nullptr)) {
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
                    }
                }
            }

            if (selectedEntity != 0 && m_Context->ecs) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                if (coordinator.IsEntityAlive(selectedEntity) && coordinator.GetSignature(selectedEntity).test(coordinator.GetComponentType<TransformComponent>())) {
                    auto& tc = coordinator.GetComponent<TransformComponent>(selectedEntity);
                    glm::vec3 glmPos(tc.position.x, tc.position.y, tc.position.z);
                    glm::quat glmRot(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);
                    glm::vec3 glmScale(tc.scale.x, tc.scale.y, tc.scale.z);

                    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glmPos) *
                                            glm::mat4_cast(glmRot) *
                                            glm::scale(glm::mat4(1.0f), glmScale);

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

            // Draw 2D Icon overlays, hybrid picking, and debug wireframes
            if (m_Context && m_Context->ecs) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                auto transformType = coordinator.GetComponentType<TransformComponent>();
                auto camType = coordinator.GetComponentType<CameraComponent>();
                auto dirLightType = coordinator.GetComponentType<DirectionalLightComponent>();
                auto pointLightType = coordinator.GetComponentType<PointLightComponent>();
                auto spotLightType = coordinator.GetComponentType<SpotLightComponent>();
                auto skyLightType = coordinator.GetComponentType<SkyLightComponent>();

                struct IconOverlay {
                    Entity entity;
                    ImVec2 screenPos;
                    std::string typeName;
                };
                std::vector<IconOverlay> iconOverlays;

                // 1. Draw debug wireframes for all entities that have colliders and debugDraw == true
                if (m_ShowColliders) {
                    auto boxType = coordinator.GetComponentType<BoxColliderComponent>();
                    auto sphereType = coordinator.GetComponentType<SphereColliderComponent>();
                    auto capsuleType = coordinator.GetComponentType<CapsuleColliderComponent>();

                    ImU32 greenColor = IM_COL32(50, 220, 50, 225);

                    for (Entity ent : coordinator.GetActiveEntities()) {
                        if (ent == 0 || !coordinator.IsEntityAlive(ent)) continue;
                        const auto& sig = coordinator.GetSignature(ent);
                        if (!sig.test(transformType)) continue;

                        const auto& tc = coordinator.GetComponent<TransformComponent>(ent);
                        glm::vec3 glmPos(tc.position.x, tc.position.y, tc.position.z);
                        glm::quat glmRot(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);
                        glm::vec3 glmScale(tc.scale.x, tc.scale.y, tc.scale.z);

                        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glmPos) *
                                                glm::mat4_cast(glmRot) *
                                                glm::scale(glm::mat4(1.0f), glmScale);

                        if (sig.test(boxType)) {
                            const auto& bc = coordinator.GetComponent<BoxColliderComponent>(ent);
                            if (bc.debugDraw) {
                                glm::vec3 offset(bc.offset.x, bc.offset.y, bc.offset.z);
                                glm::vec3 bsize(bc.size.x, bc.size.y, bc.size.z);
                                drawWireframeBox(modelMatrix, offset - bsize * 0.5f, offset + bsize * 0.5f, greenColor);
                            }
                        }
                        if (sig.test(sphereType)) {
                            const auto& sc = coordinator.GetComponent<SphereColliderComponent>(ent);
                            if (sc.debugDraw) {
                                glm::vec3 offset(sc.offset.x, sc.offset.y, sc.offset.z);
                                drawWireframeSphere(modelMatrix, offset, sc.radius, greenColor);
                            }
                        }
                        if (sig.test(capsuleType)) {
                            const auto& cc = coordinator.GetComponent<CapsuleColliderComponent>(ent);
                            if (cc.debugDraw) {
                                glm::vec3 offset(cc.offset.x, cc.offset.y, cc.offset.z);
                                drawWireframeCapsule(modelMatrix, offset, cc.radius, cc.height, greenColor);
                            }
                        }
                    }
                }

                // 2. Collect 2D icon overlays for lights and cameras
                for (Entity ent : coordinator.GetActiveEntities()) {
                    if (ent == 0 || !coordinator.IsEntityAlive(ent)) continue;
                    const auto& sig = coordinator.GetSignature(ent);
                    if (!sig.test(transformType)) continue;

                    const auto& tc = coordinator.GetComponent<TransformComponent>(ent);
                    glm::vec3 worldPos(tc.position.x, tc.position.y, tc.position.z);
                    ImVec2 screenPos;
                    if (projectWorldToScreen(worldPos, screenPos)) {
                        if (sig.test(camType)) {
                            iconOverlays.push_back({ent, screenPos, "Camera"});
                        } else if (sig.test(dirLightType)) {
                            iconOverlays.push_back({ent, screenPos, "DirectionalLight"});
                        } else if (sig.test(pointLightType)) {
                            iconOverlays.push_back({ent, screenPos, "PointLight"});
                        } else if (sig.test(spotLightType)) {
                            iconOverlays.push_back({ent, screenPos, "SpotLight"});
                        } else if (sig.test(skyLightType)) {
                            iconOverlays.push_back({ent, screenPos, "SkyLight"});
                        }
                    }
                }

                // 3. Draw 2D Icon overlays
                if (m_ShowLabels) {
                    for (const auto& icon : iconOverlays) {
                        ImU32 iconColor = IM_COL32(200, 200, 200, 255);
                        ImU32 iconBg = IM_COL32(40, 45, 55, 220);
                        const char* label = "I";
                        if (icon.typeName == "Camera") {
                            iconColor = IM_COL32(80, 160, 240, 255);
                            label = "C";
                        } else if (icon.typeName == "DirectionalLight") {
                            iconColor = IM_COL32(255, 220, 60, 255);
                            label = "D";
                        } else if (icon.typeName == "PointLight") {
                            iconColor = IM_COL32(255, 170, 40, 255);
                            label = "P";
                        } else if (icon.typeName == "SpotLight") {
                            iconColor = IM_COL32(255, 110, 30, 255);
                            label = "S";
                        } else if (icon.typeName == "SkyLight") {
                            iconColor = IM_COL32(100, 200, 255, 255);
                            label = "K";
                        }

                        if (icon.entity == selectedEntity) {
                            drawList->AddCircleFilled(icon.screenPos, 14.0f, IM_COL32(255, 140, 0, 100));
                            drawList->AddCircle(icon.screenPos, 14.0f, IM_COL32(255, 140, 0, 255), 0, 2.0f);
                        }

                        drawList->AddCircleFilled(icon.screenPos, 10.0f, iconBg);
                        drawList->AddCircle(icon.screenPos, 10.0f, iconColor, 0, 1.5f);

                        ImVec2 labelSize = ImGui::CalcTextSize(label);
                        ImVec2 labelPos(icon.screenPos.x - labelSize.x * 0.5f, icon.screenPos.y - labelSize.y * 0.5f);
                        drawList->AddText(labelPos, iconColor, label);
                    }
                }

                // 4. Draw selected light debug guides (radius/cone)
                if (m_ShowLightVolumes && selectedEntity != 0 && coordinator.IsEntityAlive(selectedEntity)) {
                    const auto& sig = coordinator.GetSignature(selectedEntity);
                    const auto& tc = coordinator.GetComponent<TransformComponent>(selectedEntity);
                    glm::vec3 worldPos(tc.position.x, tc.position.y, tc.position.z);
                    glm::quat qRot(tc.rotation.w, tc.rotation.x, tc.rotation.y, tc.rotation.z);

                    if (sig.test(pointLightType)) {
                        const auto& ptComp = coordinator.GetComponent<PointLightComponent>(selectedEntity);
                        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), worldPos);
                        drawWireframeSphere(modelMatrix, glm::vec3(0.0f), ptComp.radius, IM_COL32(255, 170, 40, 200));
                    }
                    if (sig.test(spotLightType)) {
                        const auto& spotComp = coordinator.GetComponent<SpotLightComponent>(selectedEntity);
                        drawWireframeCone(worldPos, qRot, spotComp.range, spotComp.outerConeAngle, IM_COL32(255, 110, 30, 200));
                        drawWireframeCone(worldPos, qRot, spotComp.range, spotComp.innerConeAngle, IM_COL32(255, 110, 30, 100));
                    }
                }

                // 5. Handle hybrid picking
                if (m_IsHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    Entity clickedEntity = 0;
                    float closestDist = 12.0f;

                    for (const auto& icon : iconOverlays) {
                        float dist = glm::distance(glm::vec2(mousePos.x, mousePos.y), glm::vec2(icon.screenPos.x, icon.screenPos.y));
                        if (dist < closestDist) {
                            closestDist = dist;
                            clickedEntity = icon.entity;
                        }
                    }

                    if (clickedEntity != 0) {
                        selection.Select(clickedEntity);
                    } else {
                        uint32_t clickX = static_cast<uint32_t>(mousePos.x - imageStartPos.x);
                        uint32_t clickY = static_cast<uint32_t>(mousePos.y - imageStartPos.y);

                        if (renderer) {
                            uint32_t pickedID = renderer->PickEntity(clickX, clickY);
                            if (pickedID != 0 && coordinator.IsEntityAlive(static_cast<Entity>(pickedID))) {
                                selection.Select(static_cast<Entity>(pickedID));
                            } else {
                                selection.Clear();
                            }
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
        const char* renderModes[] = {
            "Lit",
            "Preview Lit",
            "Unlit",
            "Wireframe",
            "AlbedoOnly",
            "Normal",
            "Depth",
            "Roughness",
            "Metallic",
            "AO",
            "ShadowMap",
            "Object ID",
            "Light Complexity",
            "Tangent",
            "LightingOnly",
            "LitNoOverlays",
            "UV",
            "F0",
            "Direct Diffuse",
            "Direct Specular"
        };
        auto applyRenderMode = [&](int mode) {
            m_RenderMode = mode;
            if (!renderer) {
                return;
            }
            renderer->m_UsePreviewLighting = (mode == 1);
            switch (mode) {
                case 0: renderer->m_ShadingMode = 0; break;  // Lit
                case 1: renderer->m_ShadingMode = 0; break;  // Preview Lit
                case 2: renderer->m_ShadingMode = 1; break;  // Unlit
                case 3: renderer->m_ShadingMode = 11; break; // Wireframe-style edges
                case 4: renderer->m_ShadingMode = 10; break; // AlbedoOnly
                case 5: renderer->m_ShadingMode = 3; break;  // Normal
                case 6: renderer->m_ShadingMode = 2; break;  // Depth
                case 7: renderer->m_ShadingMode = 4; break;  // Roughness
                case 8: renderer->m_ShadingMode = 5; break;  // Metallic
                case 9: renderer->m_ShadingMode = 6; break;  // AO
                case 10: renderer->m_ShadingMode = 9; break; // ShadowMap
                case 11: renderer->m_ShadingMode = 7; break; // Object ID
                case 12: renderer->m_ShadingMode = 12; break; // Light Complexity
                case 13: renderer->m_ShadingMode = 13; break; // Tangent
                case 14: renderer->m_ShadingMode = 14; break; // LightingOnly
                case 15: {
                    renderer->m_ShadingMode = 0;  // Lit
                    m_ShowGrid = false;
                    m_ShowColliders = false;
                    m_ShowBounds = false;
                    m_ShowGizmos = false;
                    m_ShowLightVolumes = false;
                    m_ShowLabels = false;
                    selection.Clear();
                    break;
                }
                case 16: renderer->m_ShadingMode = 15; break; // UV
                case 17: renderer->m_ShadingMode = 16; break; // F0
                case 18: renderer->m_ShadingMode = 17; break; // Direct Diffuse
                case 19: renderer->m_ShadingMode = 18; break; // Direct Specular
                default: renderer->m_ShadingMode = 0; break;
            }
        };

        if (renderer && m_IsFocused && !ImGui::GetIO().WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_1)) applyRenderMode(0);
            if (ImGui::IsKeyPressed(ImGuiKey_2)) applyRenderMode(2);
            if (ImGui::IsKeyPressed(ImGuiKey_3)) applyRenderMode(3);
            if (ImGui::IsKeyPressed(ImGuiKey_4)) applyRenderMode(4);
            if (ImGui::IsKeyPressed(ImGuiKey_5)) applyRenderMode(5);
            if (ImGui::IsKeyPressed(ImGuiKey_6)) applyRenderMode(6);
            if (ImGui::IsKeyPressed(ImGuiKey_7)) applyRenderMode(11);
            if (ImGui::IsKeyPressed(ImGuiKey_F12)) renderer->RequestRenderDocCapture();
        }

        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::Combo("##RenderModeCombo", &m_RenderMode, renderModes, IM_ARRAYSIZE(renderModes))) {
            applyRenderMode(m_RenderMode);
        }
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        
        {
            bool isTranslate = (m_GizmoType == ImGuizmo::TRANSLATE);
            bool isRotate = (m_GizmoType == ImGuizmo::ROTATE);
            bool isScale = (m_GizmoType == ImGuizmo::SCALE);

            if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
            if (ImGui::Button("Translate")) {
                m_GizmoType = ImGuizmo::TRANSLATE;
            }
            if (isTranslate) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
            if (ImGui::Button("Rotate")) {
                m_GizmoType = ImGuizmo::ROTATE;
            }
            if (isRotate) ImGui::PopStyleColor();

            ImGui::SameLine();
            if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
            if (ImGui::Button("Scale")) {
                m_GizmoType = ImGuizmo::SCALE;
            }
            if (isScale) ImGui::PopStyleColor();
        }

        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        
        {
            ImGui::Checkbox("Snap", &m_EnableSnapping);
            if (m_EnableSnapping) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(75.0f);
                if (m_GizmoType == ImGuizmo::TRANSLATE) {
                    ImGui::DragFloat("##SnapPos", &m_TranslationSnapValue, 0.05f, 0.05f, 10.0f, "%.2f m");
                } else if (m_GizmoType == ImGuizmo::ROTATE) {
                    ImGui::DragFloat("##SnapRot", &m_RotationSnapValue, 1.0f, 1.0f, 180.0f, "%.0f deg");
                } else if (m_GizmoType == ImGuizmo::SCALE) {
                    ImGui::DragFloat("##SnapScale", &m_ScaleSnapValue, 0.05f, 0.01f, 5.0f, "%.2f");
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
        ImGui::Checkbox("Gizmos", &m_ShowGizmos);

        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        ImGui::Checkbox("Light volumes", &m_ShowLightVolumes);

        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        ImGui::Checkbox("Labels", &m_ShowLabels);
        
        ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        
        if (ImGui::Button("Frame Selected")) {
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F, true);
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F, false);
        }

        if (renderer && renderer->m_LocalViewActive) {
            ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Isolate Active [Toggle]")) {
                renderer->m_LocalViewActive = false;
            }
            ImGui::PopStyleColor();
        }

        if (renderer) {
            ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
            auto& settings = renderer->GetRadianceSettings();
            
            ImGui::Checkbox("Auto Exposure", &settings.exposure.autoExposure);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::SliderFloat("Exposure", &settings.exposure.manualExposure, 0.05f, 8.0f, "%.2f");
            ImGui::SameLine();

            int toneMode = static_cast<int>(settings.exposure.toneMappingMode);
            const char* modes[] = { "None", "Reinhard", "ACES", "Filmic" };
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::Combo("Tone Mapping", &toneMode, modes, 4))
            {
                settings.exposure.toneMappingMode =
                    static_cast<Omnix::Radiance::ToneMappingMode>(toneMode);
            }
            ImGui::SameLine();
            ImGui::Checkbox("Before", &renderer->GetPostProcessSettings().debugBeforePostProcess);
            ImGui::SameLine();
            if (ImGui::Button("Color Grading...")) {
                ImGui::OpenPopup("ColorGradingPopup");
            }
            if (ImGui::BeginPopup("ColorGradingPopup")) {
                auto& postSettings = renderer->GetPostProcessSettings();
                ImGui::Text("Color Grading Settings");
                ImGui::Separator();
                ImGui::SliderFloat("Contrast", &postSettings.contrast, 0.5f, 1.5f);
                ImGui::SliderFloat("Saturation", &postSettings.saturation, 0.0f, 2.0f);
                ImGui::SliderFloat("Temp", &postSettings.whiteBalanceTemp, -1.0f, 1.0f);
                ImGui::SliderFloat("Tint", &postSettings.whiteBalanceTint, -1.0f, 1.0f);
                ImGui::ColorEdit3("Lift", &postSettings.lift[0]);
                ImGui::ColorEdit3("Gamma", &postSettings.gammaVal[0]);
                ImGui::ColorEdit3("Gain", &postSettings.gain[0]);
                if (ImGui::Button("Reset Color Grading")) {
                    postSettings.contrast = 1.0f;
                    postSettings.saturation = 1.0f;
                    postSettings.whiteBalanceTemp = 0.0f;
                    postSettings.whiteBalanceTint = 0.0f;
                    postSettings.lift = glm::vec3(0.0f);
                    postSettings.gammaVal = glm::vec3(1.0f);
                    postSettings.gain = glm::vec3(1.0f);
                }

                ImGui::Separator();
                ImGui::Text("Fog Settings");
                ImGui::Separator();
                bool fogEnabled = postSettings.enableFog == 1u;
                if (ImGui::Checkbox("Enable Fog", &fogEnabled)) {
                    postSettings.enableFog = fogEnabled ? 1u : 0u;
                }
                ImGui::SliderFloat("Density", &postSettings.fogDensity, 0.0f, 0.1f, "%.4f");
                ImGui::SliderFloat("Height Falloff", &postSettings.fogHeightFalloff, 0.0f, 0.5f, "%.4f");
                ImGui::SliderFloat("Base Height", &postSettings.fogBaseHeight, -50.0f, 50.0f, "%.2f");
                ImGui::ColorEdit3("Fog Color", &postSettings.fogColor[0]);

                if (ImGui::Button("Reset Fog")) {
                    postSettings.enableFog = 0u;
                    postSettings.fogDensity = 0.015f;
                    postSettings.fogHeightFalloff = 0.05f;
                    postSettings.fogBaseHeight = 0.0f;
                    postSettings.fogColor = glm::vec3(0.5f, 0.6f, 0.7f);
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
            ImGui::Text("Queue Count: %u", renderer->m_TotalRenderCount);
            ImGui::SameLine(); ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
            ImGui::Text("Transparent Count: %u", renderer->m_TransparentRenderCount);
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
            auto skyLightType = coordinator.GetComponentType<SkyLightComponent>();
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
                if (sig.test(dirLightType) || sig.test(pointLightType) || sig.test(skyLightType) || sig.test(spotLightType)) {
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
            const auto& renderStats = renderer->GetRenderStats();
            uint32_t ecsCount = 0;
            size_t sceneObjectCount = activeScene ? activeScene->GetAllSceneObjects().size() : 0;
            uint32_t directionalLights = 0;
            uint32_t pointLights = 0;
            uint32_t skyLights = 0;
            bool lightCollectionOk = false;
            std::string selectedName = "None";

            if (m_Context && m_Context->ecs) {
                auto& coordinator = m_Context->ecs->getCoordinator();
                ecsCount = coordinator.GetLivingEntityCount();
                Entity selected = selection.GetSelectedEntity();
                if (selected != 0 && coordinator.IsEntityAlive(selected)) {
                    if (coordinator.GetSignature(selected).test(coordinator.GetComponentType<NameComponent>())) {
                        selectedName = coordinator.GetComponent<NameComponent>(selected).name;
                    } else {
                        selectedName = "Entity " + std::to_string(selected);
                    }
                }

                for (Entity ent : coordinator.GetActiveEntities()) {
                    if (ent == 0 || !coordinator.IsEntityAlive(ent)) continue;
                    const auto& sig = coordinator.GetSignature(ent);
                    if (sig.test(coordinator.GetComponentType<DirectionalLightComponent>())) ++directionalLights;
                    if (sig.test(coordinator.GetComponentType<PointLightComponent>())) ++pointLights;
                    if (sig.test(coordinator.GetComponentType<SkyLightComponent>())) ++skyLights;
                }
            }

            if (renderer->m_World) {
                lightCollectionOk = renderer->m_World->getCoordinator().GetSystem<eng::runtime::LightCollectionSystem>() != nullptr;
            }

            ImGui::SetCursorScreenPos(ImVec2(imageStartPos.x + 10.0f, imageStartPos.y + size.y - 120.0f));
            
            ImGui::BeginChild("ViewportStats", ImVec2(390.0f, 600.0f), ImGuiChildFlags_Borders, 
                              ImGuiWindowFlags_None);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.9f, 1.0f));
            ImGui::Text("--- Viewport Diagnostics ---");
            ImGui::Text("Scene: %s", activeScene ? activeScene->GetName().c_str() : "Untitled");
            ImGui::Text("Path: %s", (activeScene && !activeScene->GetFilePath().empty()) ? activeScene->GetFilePath().c_str() : "Unsaved");
            ImGui::Text("Dirty: %s", dirtyState.IsSceneDirty() ? "Yes" : "No");
            ImGui::Text("Mode: %s", simulationState == EditorSimulationState::Play ? "Play" : "Edit");
            ImGui::Text("Selected: %s", selectedName.c_str());
            ImGui::Text("ECS Entities: %u", ecsCount);
            ImGui::Text("Scene Objects: %zu", sceneObjectCount);
            ImGui::Text("Input Owner: %s", m_InputOwnerLabel);
            ImGui::Text("Viewport Hovered: %s", m_IsHovered ? "Yes" : "No");
            ImGui::Text("Viewport Focused: %s", m_IsFocused ? "Yes" : "No");
            ImGui::Text("Cursor Captured: %s", m_CursorCaptured ? "Yes" : "No");
            ImGui::Text("Fallback Lighting: %s", renderer->isFallbackLightingActive() ? "ON" : "OFF");
            ImGui::Text("Directional Lights: %u", directionalLights);
            ImGui::Text("Point Lights: %u", pointLights);
            ImGui::Text("Sky Lights: %u", skyLights);
            ImGui::Text("LightCollectionSystem: %s", lightCollectionOk ? "OK" : "Missing");
            ImGui::Separator();
            ImGui::Text("Panel Size: %.0fx%.0f", outWidth, outHeight);
            ImGui::Text("Render Target: %ux%u", offscreenWidth, offscreenHeight);
            ImGui::Text("Frame Index: %u (Queue Count: %u)", frameIdx, renderQueueCount);
            ImGui::Text("Texture Handle: %s", textureValid ? "VALID" : "INVALID");
            ImGui::Separator();
            ImGui::Text("CPU Frame: %.2f ms", renderStats.cpuFrameTimeMs);
            ImGui::Text("GPU Frame: %.2f ms", renderStats.gpuFrameTimeMs);
            ImGui::Text("Draw Calls: %u", renderStats.drawCallCount);
            ImGui::Text("Visible Meshes: %u", renderStats.visibleMeshCount);
            ImGui::Text("Triangles: %u", renderStats.triangleCount);
            ImGui::Text("Materials: %u", renderStats.materialCount);
            ImGui::Text("Textures Est: %u", renderStats.textureCount);
            ImGui::Text("Lights: %u", renderStats.lightCount);
            ImGui::Text("Shadow Casters: %u", renderStats.shadowCasterCount);
            ImGui::Text("Transparent: %u", renderStats.transparentObjectCount);
            ImGui::Text("GBuffer Est: %.1f MB", renderStats.gbufferMemoryMB);
            ImGui::Text("RenderDoc: F12 capture foundation");
            
            // G3 GPU Scene Diagnostics
            ImGui::Separator();
            ImGui::Text("--- GPU Scene (G3) ---");
            auto gpuDiag = renderer->gpuScene.GetDiagnostics();
            ImGui::Text("GPU Instances: %u", gpuDiag.instanceCount);
            ImGui::Text("Active Slots: %u", gpuDiag.activeSlots);
            ImGui::Text("Free Slots: %u", gpuDiag.freeSlots);
            ImGui::Text("Upload Size: %llu bytes", gpuDiag.uploadBytesThisFrame);
            ImGui::Text("Stale Handles: %u", gpuDiag.staleHandleErrors);
            ImGui::Text("GPU Meshes: %u", gpuDiag.gpuMeshRecordCount);
            ImGui::Text("Material Overrides: %u", gpuDiag.materialOverrideCount);

            // G4 GPU-Driven Indexed Rendering Diagnostics
            ImGui::Separator();
            ImGui::Text("--- GPU Rendering (G4) ---");
            const char* modeName = "CPUDriven";
            auto mode = renderer->GetVisibilityMode();
            if (mode == eng::renderer::Renderer::VisibilityMode::GPUFrustumOnly) modeName = "GPUFrustumOnly";
            else if (mode == eng::renderer::Renderer::VisibilityMode::GPUFrustumIndirect) modeName = "GPUFrustumIndirect";
            else if (mode == eng::renderer::Renderer::VisibilityMode::GPUFrustumOcclusion) modeName = "GPUFrustumOcclusion";
            else if (mode == eng::renderer::Renderer::VisibilityMode::VisibilityBuffer) modeName = "VisibilityBuffer";
            ImGui::Text("Visibility Mode: %s", modeName);
            ImGui::Text("GPU Visible Meshes: %u", renderer->GetGpuVisibleMeshCount());
            ImGui::Text("GPU Indirect Draws: %u", renderer->GetGpuIndirectDrawCount());

            if (!renderStats.passTimings.empty()) {
                ImGui::Separator();
                ImGui::Text("GPU Pass Timings");
                for (const auto& passTiming : renderStats.passTimings) {
                    ImGui::Text("%s: %.3f ms", passTiming.name.c_str(), passTiming.gpuMs);
                }
            }
            ImGui::PopStyleColor();
            ImGui::EndChild();
        }

        ImGui::End();
    }

} // namespace eng::runtime
