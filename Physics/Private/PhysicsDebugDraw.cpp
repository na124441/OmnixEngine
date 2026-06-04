#include "Physics/Public/PhysicsDebugDraw.h"
#include "ECS/ECSComponents.h"
#include "ThirdParty/imgui/imgui.h"
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
        
        float x = (ndc.x + 1.0f) * 0.5f * screenWidth;
        float y = (1.0f - ndc.y) * 0.5f * screenHeight;
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

    void PhysicsDebugDraw::Render(Coordinator& coordinator, const glm::mat4& view, const glm::mat4& proj, float screenWidth, float screenHeight) {
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
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
            if (signature.test(coordinator.GetComponentType<BoxColliderComponent>())) {
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
            if (signature.test(coordinator.GetComponentType<SphereColliderComponent>())) {
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
            if (signature.test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
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
    }

    std::vector<DebugRaycastInfo> PhysicsDebugDraw::s_Raycasts;

    void PhysicsDebugDraw::AddDebugRaycast(const Vector3& origin, const Vector3& direction, float distance, bool hit, const Vector3& hitPoint, const Vector3& hitNormal) {
        s_Raycasts.push_back({origin, direction, distance, hit, hitPoint, hitNormal});
    }

    void PhysicsDebugDraw::ClearDebugVisuals() {
        s_Raycasts.clear();
    }
}
