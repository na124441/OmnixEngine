#include "Physics/Public/PhysicsDebugDraw.h"
#include "ECS/ECSComponents.h"
#include "ECS/TriggerSystem.h"
#include "ThirdParty/imgui/imgui.h"
#include "Rendering/Debug/DebugDraw.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace eng::physics {

    static glm::mat4 ToGlmMatrix(const Matrix4x4& mat) {
        glm::mat4 result;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                result[col][row] = mat.m[col * 4 + row];
            }
        }
        return result;
    }

    // Viewport offset: top-left corner of the viewport panel in screen space
    static float s_ViewportOffsetX = 0.0f;
    static float s_ViewportOffsetY = 0.0f;

    static glm::vec2 ProjectPoint(const glm::vec3& localPoint, const glm::mat4& entityTransform, const glm::mat4& view, const glm::mat4& proj, float screenWidth, float screenHeight, bool& outBehindCamera) {
        glm::vec4 worldPos = entityTransform * glm::vec4(localPoint, 1.0f);
        glm::vec4 viewPos = view * worldPos;
        glm::vec4 clipPos = proj * viewPos;
        
        if (clipPos.w <= 0.001f) {
            outBehindCamera = true;
            return glm::vec2(0.0f);
        }
        outBehindCamera = false;
        
        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
        
        float x = s_ViewportOffsetX + (ndc.x + 1.0f) * 0.5f * screenWidth;
        float y = s_ViewportOffsetY + (1.0f - ndc.y) * 0.5f * screenHeight;
        return glm::vec2(x, y);
    }

    static void DrawCircle(const glm::vec3& center, const glm::vec3& axis1, const glm::vec3& axis2, float radius, const glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj, float width, float height, ImDrawList* drawList, ImU32 color) {
        const int segments = 16;
        glm::vec3 prevPoint;
        bool hasPrev = false;
        glm::vec3 firstPoint;
        bool hasFirst = false;

        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
            glm::vec3 p = center + (axis1 * cosf(angle) + axis2 * sinf(angle)) * radius;
            if (i == 0) {
                firstPoint = p;
                hasFirst = true;
            }
            if (hasPrev) {
                bool behind1 = false, behind2 = false;
                glm::vec2 p1 = ProjectPoint(prevPoint, transform, view, proj, width, height, behind1);
                glm::vec2 p2 = ProjectPoint(p, transform, view, proj, width, height, behind2);
                if (!behind1 && !behind2) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 2.0f);
                }
            }
            prevPoint = p;
            hasPrev = true;
        }
    }

    static void DrawArc(const glm::vec3& center, const glm::vec3& axis1, const glm::vec3& axis2, float radius, float startAngle, float endAngle, const glm::mat4& transform, const glm::mat4& view, const glm::mat4& proj, float width, float height, ImDrawList* drawList, ImU32 color) {
        const int segments = 8;
        glm::vec3 prevPoint;
        bool hasPrev = false;

        for (int i = 0; i <= segments; ++i) {
            float angle = startAngle + (float)i * (endAngle - startAngle) / (float)segments;
            glm::vec3 p = center + (axis1 * cosf(angle) + axis2 * sinf(angle)) * radius;
            if (hasPrev) {
                bool behind1 = false, behind2 = false;
                glm::vec2 p1 = ProjectPoint(prevPoint, transform, view, proj, width, height, behind1);
                glm::vec2 p2 = ProjectPoint(p, transform, view, proj, width, height, behind2);
                if (!behind1 && !behind2) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 2.0f);
                }
            }
            prevPoint = p;
            hasPrev = true;
        }
    }

    void PhysicsDebugDraw::Render(
        Coordinator& coordinator,
        const glm::mat4& view,
        const glm::mat4& proj,
        float screenWidth,
        float screenHeight,
        float viewportOffsetX,
        float viewportOffsetY,
        Entity selectedEntity,
        bool showColliders,
        bool showLights,
        bool showBounds,
        const glm::mat4& shadowMatrix
    ) {
        s_ViewportOffsetX = viewportOffsetX;
        s_ViewportOffsetY = viewportOffsetY;
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (!drawList) return;

        ImU32 colliderColor = IM_COL32(0, 255, 0, 255); // Green for standard colliders
        ImU32 triggerColor = IM_COL32(255, 255, 0, 255); // Yellow for triggers

        // Loop over all active entities in coordinator
        for (Entity entity : coordinator.GetActiveEntities()) {
            if (entity == 0 || !coordinator.IsEntityAlive(entity)) continue;

            auto signature = coordinator.GetSignature(entity);
            if (!signature.test(coordinator.GetComponentType<TransformComponent>())) continue;

            const auto& tc = coordinator.GetComponent<TransformComponent>(entity);
            Matrix4x4 trs = Matrix4x4::TRS(tc.position, tc.rotation, tc.scale);
            glm::mat4 entityTransform = ToGlmMatrix(trs);

            // 1. Box Collider
            if (showColliders && signature.test(coordinator.GetComponentType<BoxColliderComponent>())) {
                const auto& box = coordinator.GetComponent<BoxColliderComponent>(entity);
                if (box.debugDraw) {
                    ImU32 color = box.isTrigger ? triggerColor : colliderColor;
                    glm::vec3 halfSize = glm::vec3(box.size.x, box.size.y, box.size.z) * 0.5f;
                    glm::vec3 offset = glm::vec3(box.offset.x, box.offset.y, box.offset.z);

                    glm::vec3 v[8];
                    v[0] = offset + glm::vec3(-halfSize.x, -halfSize.y, -halfSize.z);
                    v[1] = offset + glm::vec3( halfSize.x, -halfSize.y, -halfSize.z);
                    v[2] = offset + glm::vec3( halfSize.x,  halfSize.y, -halfSize.z);
                    v[3] = offset + glm::vec3(-halfSize.x,  halfSize.y, -halfSize.z);
                    v[4] = offset + glm::vec3(-halfSize.x, -halfSize.y,  halfSize.z);
                    v[5] = offset + glm::vec3( halfSize.x, -halfSize.y,  halfSize.z);
                    v[6] = offset + glm::vec3( halfSize.x,  halfSize.y,  halfSize.z);
                    v[7] = offset + glm::vec3(-halfSize.x,  halfSize.y,  halfSize.z);

                    int indices[12][2] = {
                        {0,1}, {1,2}, {2,3}, {3,0}, // Bottom face
                        {4,5}, {5,6}, {6,7}, {7,4}, // Top face
                        {0,4}, {1,5}, {2,6}, {3,7}  // Vertical edges
                    };

                    for (int i = 0; i < 12; ++i) {
                        bool behind1 = false, behind2 = false;
                        glm::vec2 p1 = ProjectPoint(v[indices[i][0]], entityTransform, view, proj, screenWidth, screenHeight, behind1);
                        glm::vec2 p2 = ProjectPoint(v[indices[i][1]], entityTransform, view, proj, screenWidth, screenHeight, behind2);
                        if (!behind1 && !behind2) {
                            drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 2.0f);
                        }
                    }
                }
            }

            // 2. Sphere Collider
            if (showColliders && signature.test(coordinator.GetComponentType<SphereColliderComponent>())) {
                const auto& sphere = coordinator.GetComponent<SphereColliderComponent>(entity);
                if (sphere.debugDraw) {
                    ImU32 color = sphere.isTrigger ? triggerColor : colliderColor;
                    glm::vec3 center = glm::vec3(sphere.offset.x, sphere.offset.y, sphere.offset.z);
                    float r = sphere.radius;

                    DrawCircle(center, glm::vec3(1,0,0), glm::vec3(0,1,0), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // XY
                    DrawCircle(center, glm::vec3(1,0,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // XZ
                    DrawCircle(center, glm::vec3(0,1,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // YZ
                }
            }

            // 3. Capsule Collider
            if (showColliders && signature.test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
                const auto& capsule = coordinator.GetComponent<CapsuleColliderComponent>(entity);
                if (capsule.debugDraw) {
                    ImU32 color = capsule.isTrigger ? triggerColor : colliderColor;
                    glm::vec3 offset = glm::vec3(capsule.offset.x, capsule.offset.y, capsule.offset.z);
                    float r = capsule.radius;
                    float cylinderHeight = capsule.height - 2.0f * r;
                    if (cylinderHeight < 0.0f) cylinderHeight = 0.0f;
                    float halfCylHeight = cylinderHeight * 0.5f;

                    // Vertical lines
                    glm::vec3 v1_top = offset + glm::vec3(r, halfCylHeight, 0);
                    glm::vec3 v1_bot = offset + glm::vec3(r, -halfCylHeight, 0);
                    glm::vec3 v2_top = offset + glm::vec3(-r, halfCylHeight, 0);
                    glm::vec3 v2_bot = offset + glm::vec3(-r, -halfCylHeight, 0);
                    glm::vec3 v3_top = offset + glm::vec3(0, halfCylHeight, r);
                    glm::vec3 v3_bot = offset + glm::vec3(0, -halfCylHeight, r);
                    glm::vec3 v4_top = offset + glm::vec3(0, halfCylHeight, -r);
                    glm::vec3 v4_bot = offset + glm::vec3(0, -halfCylHeight, -r);

                    glm::vec3 lines[4][2] = {
                        {v1_top, v1_bot},
                        {v2_top, v2_bot},
                        {v3_top, v3_bot},
                        {v4_top, v4_bot}
                    };

                    for (int i = 0; i < 4; ++i) {
                        bool behind1 = false, behind2 = false;
                        glm::vec2 p1 = ProjectPoint(lines[i][0], entityTransform, view, proj, screenWidth, screenHeight, behind1);
                        glm::vec2 p2 = ProjectPoint(lines[i][1], entityTransform, view, proj, screenWidth, screenHeight, behind2);
                        if (!behind1 && !behind2) {
                            drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 2.0f);
                        }
                    }

                    // Top & bottom bands (XZ circles)
                    DrawCircle(offset + glm::vec3(0, halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                    DrawCircle(offset + glm::vec3(0, -halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);

                    // Hemisphere arches
                    // Top dome: XY and YZ semicircles
                    DrawArc(offset + glm::vec3(0, halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                    DrawArc(offset + glm::vec3(0, halfCylHeight, 0), glm::vec3(0,0,1), glm::vec3(0,1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);

                    // Bottom dome: XY and YZ semicircles
                    DrawArc(offset + glm::vec3(0, -halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,-1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                    DrawArc(offset + glm::vec3(0, -halfCylHeight, 0), glm::vec3(0,0,1), glm::vec3(0,-1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                }
            }

            // 4. Trigger Volumes
            if (showColliders && signature.test(coordinator.GetComponentType<TriggerComponent>())) {
                const auto& trigger = coordinator.GetComponent<TriggerComponent>(entity);
                if (trigger.enabled) {
                    ImU32 color = IM_COL32(255, 255, 0, 255); // Yellow default

                    auto triggerSys = coordinator.GetSystem<eng::runtime::TriggerSystem>();
                    if (triggerSys) {
                        if (!triggerSys->IsDimensionsValid(trigger)) {
                            color = IM_COL32(255, 0, 0, 255); // Red for invalid
                        } else if (triggerSys->IsTriggerActive(entity)) {
                            color = IM_COL32(0, 255, 0, 255); // Green for active
                        }
                    }

                    glm::vec3 offset = glm::vec3(trigger.offset.x, trigger.offset.y, trigger.offset.z);

                    if (trigger.shapeType == TriggerShapeType::Box) {
                        glm::vec3 halfSize = glm::vec3(trigger.boxSize.x, trigger.boxSize.y, trigger.boxSize.z) * 0.5f;
                        glm::vec3 v[8];
                        v[0] = offset + glm::vec3(-halfSize.x, -halfSize.y, -halfSize.z);
                        v[1] = offset + glm::vec3( halfSize.x, -halfSize.y, -halfSize.z);
                        v[2] = offset + glm::vec3( halfSize.x,  halfSize.y, -halfSize.z);
                        v[3] = offset + glm::vec3(-halfSize.x,  halfSize.y, -halfSize.z);
                        v[4] = offset + glm::vec3(-halfSize.x, -halfSize.y,  halfSize.z);
                        v[5] = offset + glm::vec3( halfSize.x, -halfSize.y,  halfSize.z);
                        v[6] = offset + glm::vec3( halfSize.x,  halfSize.y,  halfSize.z);
                        v[7] = offset + glm::vec3(-halfSize.x,  halfSize.y,  halfSize.z);

                        int indices[12][2] = {
                            {0,1}, {1,2}, {2,3}, {3,0},
                            {4,5}, {5,6}, {6,7}, {7,4},
                            {0,4}, {1,5}, {2,6}, {3,7}
                        };

                        for (int i = 0; i < 12; ++i) {
                            bool behind1 = false, behind2 = false;
                            glm::vec2 p1 = ProjectPoint(v[indices[i][0]], entityTransform, view, proj, screenWidth, screenHeight, behind1);
                            glm::vec2 p2 = ProjectPoint(v[indices[i][1]], entityTransform, view, proj, screenWidth, screenHeight, behind2);
                            if (!behind1 && !behind2) {
                                drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 2.0f);
                            }
                        }
                    }
                    else if (trigger.shapeType == TriggerShapeType::Sphere) {
                        float r = trigger.sphereRadius;
                        DrawCircle(offset, glm::vec3(1,0,0), glm::vec3(0,1,0), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // XY
                        DrawCircle(offset, glm::vec3(1,0,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // XZ
                        DrawCircle(offset, glm::vec3(0,1,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // YZ
                    }
                    else if (trigger.shapeType == TriggerShapeType::Capsule) {
                        float r = trigger.capsuleRadius;
                        float cylinderHeight = trigger.capsuleHeight - 2.0f * r;
                        if (cylinderHeight < 0.0f) cylinderHeight = 0.0f;
                        float halfCylHeight = cylinderHeight * 0.5f;

                        glm::vec3 v1_top = offset + glm::vec3(r, halfCylHeight, 0);
                        glm::vec3 v1_bot = offset + glm::vec3(r, -halfCylHeight, 0);
                        glm::vec3 v2_top = offset + glm::vec3(-r, halfCylHeight, 0);
                        glm::vec3 v2_bot = offset + glm::vec3(-r, -halfCylHeight, 0);
                        glm::vec3 v3_top = offset + glm::vec3(0, halfCylHeight, r);
                        glm::vec3 v3_bot = offset + glm::vec3(0, -halfCylHeight, r);
                        glm::vec3 v4_top = offset + glm::vec3(0, halfCylHeight, -r);
                        glm::vec3 v4_bot = offset + glm::vec3(0, -halfCylHeight, -r);

                        glm::vec3 lines[4][2] = {
                            {v1_top, v1_bot},
                            {v2_top, v2_bot},
                            {v3_top, v3_bot},
                            {v4_top, v4_bot}
                        };

                        for (int i = 0; i < 4; ++i) {
                            bool behind1 = false, behind2 = false;
                            glm::vec2 p1 = ProjectPoint(lines[i][0], entityTransform, view, proj, screenWidth, screenHeight, behind1);
                            glm::vec2 p2 = ProjectPoint(lines[i][1], entityTransform, view, proj, screenWidth, screenHeight, behind2);
                            if (!behind1 && !behind2) {
                                drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 2.0f);
                            }
                        }

                        DrawCircle(offset + glm::vec3(0, halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                        DrawCircle(offset + glm::vec3(0, -halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);

                        DrawArc(offset + glm::vec3(0, halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                        DrawArc(offset + glm::vec3(0, halfCylHeight, 0), glm::vec3(0,0,1), glm::vec3(0,1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);

                        DrawArc(offset + glm::vec3(0, -halfCylHeight, 0), glm::vec3(1,0,0), glm::vec3(0,-1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                        DrawArc(offset + glm::vec3(0, -halfCylHeight, 0), glm::vec3(0,0,1), glm::vec3(0,-1,0), r, 0.0f, 3.14159265f, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                    }
                }
            }

            // 5. Directional Light Debug Visuals
            if (showLights && signature.test(coordinator.GetComponentType<DirectionalLightComponent>())) {
                const auto& dirLight = coordinator.GetComponent<DirectionalLightComponent>(entity);
                if (dirLight.enabled) {
                    ImU32 color = IM_COL32(255, 255, 0, 255); // Yellow
                    bool drawBounds = (entity == selectedEntity) || showBounds;
                    if (drawBounds) {
                        glm::vec3 start = glm::vec3(0.0f);
                        glm::vec3 end = glm::vec3(0.0f, 0.0f, -2.0f);
                        bool behindStart = false, behindEnd = false;
                        glm::vec2 pStart = ProjectPoint(start, entityTransform, view, proj, screenWidth, screenHeight, behindStart);
                        glm::vec2 pEnd = ProjectPoint(end, entityTransform, view, proj, screenWidth, screenHeight, behindEnd);
                        if (!behindStart && !behindEnd) {
                            drawList->AddLine(ImVec2(pStart.x, pStart.y), ImVec2(pEnd.x, pEnd.y), color, 2.0f);
                        }
                        // Draw arrowhead lines
                        glm::vec3 arrowHead[4] = {
                            end + glm::vec3(0.2f, 0.0f, 0.4f),
                            end + glm::vec3(-0.2f, 0.0f, 0.4f),
                            end + glm::vec3(0.0f, 0.2f, 0.4f),
                            end + glm::vec3(0.0f, -0.2f, 0.4f)
                        };
                        for (int i = 0; i < 4; ++i) {
                            bool behindHead = false;
                            glm::vec2 pHead = ProjectPoint(arrowHead[i], entityTransform, view, proj, screenWidth, screenHeight, behindHead);
                            if (!behindEnd && !behindHead) {
                                drawList->AddLine(ImVec2(pEnd.x, pEnd.y), ImVec2(pHead.x, pHead.y), color, 2.0f);
                            }
                        }
                        
                        // Draw shadow frustum wireframe
                        if (dirLight.castShadows && std::abs(glm::determinant(shadowMatrix)) > 0.0001f) {
                            glm::mat4 invShadow = glm::inverse(shadowMatrix);
                            glm::vec3 ndcCorners[8] = {
                                {-1.0f, -1.0f, 0.0f},
                                { 1.0f, -1.0f, 0.0f},
                                {-1.0f,  1.0f, 0.0f},
                                { 1.0f,  1.0f, 0.0f},
                                {-1.0f, -1.0f, 1.0f},
                                { 1.0f, -1.0f, 1.0f},
                                {-1.0f,  1.0f, 1.0f},
                                { 1.0f,  1.0f, 1.0f}
                            };
                            glm::vec3 worldCorners[8];
                            for (int i = 0; i < 8; ++i) {
                                glm::vec4 pt = invShadow * glm::vec4(ndcCorners[i], 1.0f);
                                worldCorners[i] = glm::vec3(pt) / pt.w;
                            }
                            int edges[12][2] = {
                                {0, 1}, {1, 3}, {3, 2}, {2, 0}, // Near
                                {4, 5}, {5, 7}, {7, 6}, {6, 4}, // Far
                                {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connections
                            };
                            ImU32 shadowFrustumColor = IM_COL32(255, 128, 0, 255); // Orange
                            for (int i = 0; i < 12; ++i) {
                                bool behind1 = false, behind2 = false;
                                glm::vec2 p1 = ProjectPoint(worldCorners[edges[i][0]], glm::mat4(1.0f), view, proj, screenWidth, screenHeight, behind1);
                                glm::vec2 p2 = ProjectPoint(worldCorners[edges[i][1]], glm::mat4(1.0f), view, proj, screenWidth, screenHeight, behind2);
                                if (!behind1 && !behind2) {
                                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), shadowFrustumColor, 1.5f);
                                }
                            }
                        }
                    }
                    bool behindStart = false;
                    glm::vec2 pStart = ProjectPoint(glm::vec3(0.0f), entityTransform, view, proj, screenWidth, screenHeight, behindStart);
                    if (!behindStart) {
                        drawList->AddCircle(ImVec2(pStart.x, pStart.y), 6.0f, color, 8, 2.0f);
                        drawList->AddText(ImVec2(pStart.x + 10.0f, pStart.y - 10.0f), color, "[Directional Light]");
                    }
                }
            }

            // 6. Point Light Debug Visuals
            if (showLights && signature.test(coordinator.GetComponentType<PointLightComponent>())) {
                const auto& ptLight = coordinator.GetComponent<PointLightComponent>(entity);
                if (ptLight.enabled) {
                    ImU32 color = IM_COL32(255, 128, 0, 255); // Orange
                    bool drawBounds = (entity == selectedEntity) || showBounds;
                    if (drawBounds) {
                        float r = ptLight.radius;
                        DrawCircle(glm::vec3(0.0f), glm::vec3(1,0,0), glm::vec3(0,1,0), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // XY
                        DrawCircle(glm::vec3(0.0f), glm::vec3(1,0,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // XZ
                        DrawCircle(glm::vec3(0.0f), glm::vec3(0,1,0), glm::vec3(0,0,1), r, entityTransform, view, proj, screenWidth, screenHeight, drawList, color); // YZ
                    }
                    bool behind = false;
                    glm::vec2 pCenter = ProjectPoint(glm::vec3(0.0f), entityTransform, view, proj, screenWidth, screenHeight, behind);
                    if (!behind) {
                        drawList->AddCircle(ImVec2(pCenter.x, pCenter.y), 6.0f, color, 8, 2.0f);
                        drawList->AddText(ImVec2(pCenter.x + 10.0f, pCenter.y - 10.0f), color, "[Point Light]");
                    }
                }
            }

            // 7. Sky Light Debug Visuals
            if (showLights && signature.test(coordinator.GetComponentType<SkyLightComponent>())) {
                const auto& skyLight = coordinator.GetComponent<SkyLightComponent>(entity);
                if (skyLight.enabled) {
                    ImU32 color = IM_COL32(0, 128, 255, 255); // Blue
                    bool behind = false;
                    glm::vec2 pCenter = ProjectPoint(glm::vec3(0.0f), entityTransform, view, proj, screenWidth, screenHeight, behind);
                    if (!behind) {
                        drawList->AddCircle(ImVec2(pCenter.x, pCenter.y), 6.0f, color, 8, 2.0f);
                        drawList->AddText(ImVec2(pCenter.x + 10.0f, pCenter.y - 10.0f), color, "[Sky Light]");
                    }
                }
            }

            // 8. Spot Light Debug Visuals
            if (showLights && signature.test(coordinator.GetComponentType<SpotLightComponent>())) {
                const auto& spotLight = coordinator.GetComponent<SpotLightComponent>(entity);
                if (spotLight.enabled) {
                    ImU32 color = IM_COL32(255, 255, 255, 255); // White
                    bool drawBounds = (entity == selectedEntity) || showBounds;
                    if (drawBounds) {
                        float r = spotLight.range;
                        float angleRad = glm::radians(spotLight.outerConeAngle);
                        float baseRadius = r * tanf(angleRad);
                        glm::vec3 tip = glm::vec3(0.0f);
                        glm::vec3 baseCenter = glm::vec3(0.0f, 0.0f, -r);
                        DrawCircle(baseCenter, glm::vec3(1,0,0), glm::vec3(0,1,0), baseRadius, entityTransform, view, proj, screenWidth, screenHeight, drawList, color);
                        
                        glm::vec3 directions[4] = {
                            glm::vec3(baseRadius, 0.0f, -r),
                            glm::vec3(-baseRadius, 0.0f, -r),
                            glm::vec3(0.0f, baseRadius, -r),
                            glm::vec3(0.0f, -baseRadius, -r)
                        };
                        for (int i = 0; i < 4; ++i) {
                            bool behindTip = false, behindBase = false;
                            glm::vec2 pTip = ProjectPoint(tip, entityTransform, view, proj, screenWidth, screenHeight, behindTip);
                            glm::vec2 pBase = ProjectPoint(directions[i], entityTransform, view, proj, screenWidth, screenHeight, behindBase);
                            if (!behindTip && !behindBase) {
                                drawList->AddLine(ImVec2(pTip.x, pTip.y), ImVec2(pBase.x, pBase.y), color, 2.0f);
                            }
                        }

                        // Draw inner cone
                        float innerAngleRad = glm::radians(spotLight.innerConeAngle);
                        float innerBaseRadius = r * tanf(innerAngleRad);
                        glm::vec3 innerBaseCenter = glm::vec3(0.0f, 0.0f, -r);
                        ImU32 innerColor = IM_COL32(200, 200, 255, 180); // Softer white-blue for inner cone
                        DrawCircle(innerBaseCenter, glm::vec3(1,0,0), glm::vec3(0,1,0), innerBaseRadius, entityTransform, view, proj, screenWidth, screenHeight, drawList, innerColor);
                        
                        glm::vec3 innerDirections[4] = {
                            glm::vec3(innerBaseRadius, 0.0f, -r),
                            glm::vec3(-innerBaseRadius, 0.0f, -r),
                            glm::vec3(0.0f, innerBaseRadius, -r),
                            glm::vec3(0.0f, -innerBaseRadius, -r)
                        };
                        for (int i = 0; i < 4; ++i) {
                            bool behindTip = false, behindBase = false;
                            glm::vec2 pTip = ProjectPoint(tip, entityTransform, view, proj, screenWidth, screenHeight, behindTip);
                            glm::vec2 pBase = ProjectPoint(innerDirections[i], entityTransform, view, proj, screenWidth, screenHeight, behindBase);
                            if (!behindTip && !behindBase) {
                                drawList->AddLine(ImVec2(pTip.x, pTip.y), ImVec2(pBase.x, pBase.y), innerColor, 1.0f);
                            }
                        }
                    }
                    bool behindTip = false;
                    glm::vec2 pTip = ProjectPoint(glm::vec3(0.0f), entityTransform, view, proj, screenWidth, screenHeight, behindTip);
                    if (!behindTip) {
                        drawList->AddCircle(ImVec2(pTip.x, pTip.y), 6.0f, color, 8, 2.0f);
                        drawList->AddText(ImVec2(pTip.x + 10.0f, pTip.y - 10.0f), color, "[Spot Light]");
                    }
                }
            }
        }

        // Render cached debug raycasts
        for (const auto& ray : s_Raycasts) {
            glm::vec3 rayStart = glm::vec3(ray.origin.x, ray.origin.y, ray.origin.z);
            glm::vec3 normDir = glm::normalize(glm::vec3(ray.direction.x, ray.direction.y, ray.direction.z));
            glm::vec3 rayEnd;
            if (ray.hit) {
                rayEnd = glm::vec3(ray.hitPoint.x, ray.hitPoint.y, ray.hitPoint.z);
            } else {
                rayEnd = rayStart + normDir * ray.distance;
            }

            bool startBehind = false, endBehind = false;
            glm::vec2 screenStart = ProjectPoint(rayStart, glm::mat4(1.0f), view, proj, screenWidth, screenHeight, startBehind);
            glm::vec2 screenEnd = ProjectPoint(rayEnd, glm::mat4(1.0f), view, proj, screenWidth, screenHeight, endBehind);

            ImU32 rayColor = ray.hit ? IM_COL32(0, 255, 255, 255) : IM_COL32(255, 0, 0, 255); // Cyan for hit, Red for miss

            if (!startBehind && !endBehind) {
                drawList->AddLine(ImVec2(screenStart.x, screenStart.y), ImVec2(screenEnd.x, screenEnd.y), rayColor, 2.0f);
            }

            if (ray.hit) {
                // Draw hit circle marker
                bool hitBehind = false;
                glm::vec2 screenHit = ProjectPoint(rayEnd, glm::mat4(1.0f), view, proj, screenWidth, screenHeight, hitBehind);
                if (!hitBehind) {
                    drawList->AddCircleFilled(ImVec2(screenHit.x, screenHit.y), 5.0f, rayColor);
                    // Also draw normal line in Cyan
                    glm::vec3 normEnd = rayEnd + glm::vec3(ray.hitNormal.x, ray.hitNormal.y, ray.hitNormal.z) * 1.5f; // Project normal out by 1.5 units
                    bool normBehind = false;
                    glm::vec2 screenNormEnd = ProjectPoint(normEnd, glm::mat4(1.0f), view, proj, screenWidth, screenHeight, normBehind);
                    if (!normBehind) {
                        drawList->AddLine(ImVec2(screenHit.x, screenHit.y), ImVec2(screenNormEnd.x, screenNormEnd.y), IM_COL32(0, 255, 255, 255), 2.0f);
                    }
                }
            }
        }

        // 9. Render custom DebugDraw shapes (e.g. GPU local lights debug shapes)
        const auto& debugLines = eng::renderer::DebugDraw::GetLines();
        for (const auto& line : debugLines) {
            bool behind1 = false, behind2 = false;
            glm::vec2 p1 = ProjectPoint(line.p1, glm::mat4(1.0f), view, proj, screenWidth, screenHeight, behind1);
            glm::vec2 p2 = ProjectPoint(line.p2, glm::mat4(1.0f), view, proj, screenWidth, screenHeight, behind2);
            if (!behind1 && !behind2) {
                ImU32 color = IM_COL32(
                    static_cast<int>(line.color.r * 255.0f),
                    static_cast<int>(line.color.g * 255.0f),
                    static_cast<int>(line.color.b * 255.0f),
                    static_cast<int>(line.color.a * 255.0f)
                );
                drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 2.0f);
            }
        }
        eng::renderer::DebugDraw::ClearLines();
    }

    // -------------------------------------------------------------------------
    // RenderBounds — draws world-space AABBs (and optional bounding spheres)
    // for every entity that has a BoundsComponent.
    // -------------------------------------------------------------------------
    void PhysicsDebugDraw::RenderBounds(Coordinator& coordinator, const glm::mat4& view, const glm::mat4& proj, float screenWidth, float screenHeight, float viewportOffsetX, float viewportOffsetY) {
        s_ViewportOffsetX = viewportOffsetX;
        s_ViewportOffsetY = viewportOffsetY;
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (!drawList) return;

        ImU32 aabbColor   = IM_COL32(0, 220, 255, 200); // Cyan — AABB
        ImU32 sphereColor = IM_COL32(255, 200, 0, 160); // Amber — sphere

        // World-space AABB corners are already in world space, so use identity transform.
        glm::mat4 identity = glm::mat4(1.0f);

        for (Entity entity : coordinator.GetActiveEntities()) {
            if (entity == 0 || !coordinator.IsEntityAlive(entity)) continue;

            auto signature = coordinator.GetSignature(entity);
            if (!signature.test(coordinator.GetComponentType<BoundsComponent>())) continue;

            const auto& bounds = coordinator.GetComponent<BoundsComponent>(entity);

            // Skip degenerate (all-zero) world bounds
            if (bounds.worldMin.x == bounds.worldMax.x &&
                bounds.worldMin.y == bounds.worldMax.y &&
                bounds.worldMin.z == bounds.worldMax.z) {
                continue;
            }

            // Build 8 world-space corners of the AABB
            glm::vec3 wMin(bounds.worldMin.x, bounds.worldMin.y, bounds.worldMin.z);
            glm::vec3 wMax(bounds.worldMax.x, bounds.worldMax.y, bounds.worldMax.z);

            glm::vec3 v[8] = {
                { wMin.x, wMin.y, wMin.z },
                { wMax.x, wMin.y, wMin.z },
                { wMax.x, wMax.y, wMin.z },
                { wMin.x, wMax.y, wMin.z },
                { wMin.x, wMin.y, wMax.z },
                { wMax.x, wMin.y, wMax.z },
                { wMax.x, wMax.y, wMax.z },
                { wMin.x, wMax.y, wMax.z }
            };

            // 12 edges of the AABB
            int edges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},  // near face
                {4,5},{5,6},{6,7},{7,4},  // far face
                {0,4},{1,5},{2,6},{3,7}   // connecting edges
            };

            for (int i = 0; i < 12; ++i) {
                bool behind1 = false, behind2 = false;
                glm::vec2 p1 = ProjectPoint(v[edges[i][0]], identity, view, proj, screenWidth, screenHeight, behind1);
                glm::vec2 p2 = ProjectPoint(v[edges[i][1]], identity, view, proj, screenWidth, screenHeight, behind2);
                if (!behind1 && !behind2) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), aabbColor, 1.5f);
                }
            }

            // Optional bounding sphere (3 great circles)
            if (bounds.hasSphere) {
                glm::vec3 sc(bounds.worldSphereCenter.x, bounds.worldSphereCenter.y, bounds.worldSphereCenter.z);
                float sr = bounds.worldSphereRadius;
                DrawCircle(sc, glm::vec3(1,0,0), glm::vec3(0,1,0), sr, identity, view, proj, screenWidth, screenHeight, drawList, sphereColor);
                DrawCircle(sc, glm::vec3(1,0,0), glm::vec3(0,0,1), sr, identity, view, proj, screenWidth, screenHeight, drawList, sphereColor);
                DrawCircle(sc, glm::vec3(0,1,0), glm::vec3(0,0,1), sr, identity, view, proj, screenWidth, screenHeight, drawList, sphereColor);
            }
        }
    }

    std::vector<DebugRaycastInfo> PhysicsDebugDraw::s_Raycasts;

    void PhysicsDebugDraw::AddDebugRaycast(const Vector3& origin, const Vector3& direction, float distance, bool hit, const Vector3& hitPoint, const Vector3& hitNormal) {
        s_Raycasts.push_back({origin, direction, distance, hit, hitPoint, hitNormal});
    }

    void PhysicsDebugDraw::ClearDebugVisuals() {
        s_Raycasts.clear();
        eng::renderer::DebugDraw::ClearLines();
    }
}
