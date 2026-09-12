//============================================================================
// PlayerControllerSystem.h - First Person Player Controller System
//============================================================================

#pragma once

#include "SystemManager.h"
#include "ECSComponents.h"
#include "Coordinator.h"
#include "Physics/Public/PhysicsWorld.h"
#include "Physics/Public/PhysicsQueries.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"
#include "ThirdParty/imgui/imgui.h"
#include "Input/InputManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <vector>
#include <iostream>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace eng::runtime {

    class PlayerControllerSystem : public System {
    public:
        void SetPlayerEntity(Entity playerEntity) {
            m_PlayerEntity = playerEntity;
        }

        Entity GetPlayerEntity() const {
            return m_PlayerEntity;
        }

        // Helper to check capsule overlaps, ignoring the player entity
        bool OverlapCapsuleFiltered(eng::physics::PhysicsWorld* physicsWorld, const Vector3& center, float radius, float height, Entity playerEntity) {
            if (!physicsWorld) return false;
            std::vector<Entity> overlapped;
            if (physicsWorld->OverlapCapsule(center, radius, height, overlapped)) {
                for (Entity e : overlapped) {
                    if (e != playerEntity) {
                        return true; // Overlaps with an actual obstacle
                    }
                }
            }
            return false;
        }

        void FixedUpdate(eng::physics::PhysicsWorld* physicsWorld, Coordinator& coordinator, float fixedDeltaTime, InputManager* input = nullptr) {
            if (m_PlayerEntity == 0 || !coordinator.IsEntityAlive(m_PlayerEntity)) return;

            // Check if ZoneEntityComponent is attached and simulating is false
            auto signature = coordinator.GetSignature(m_PlayerEntity);
            if (signature.test(coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>())) {
                const auto& zec = coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(m_PlayerEntity);
                if (!zec.simulating) {
                    return;
                }
            }

            auto& transform = coordinator.GetComponent<TransformComponent>(m_PlayerEntity);
            auto& ccc = coordinator.GetComponent<CharacterControllerComponent>(m_PlayerEntity);

            // 1. Calculate desired horizontal movement based on input (using yaw)
            float yawRad = glm::radians(ccc.yaw);
            // Forward is X = cos, Z = sin
            glm::vec3 f(std::cos(yawRad), 0.0f, std::sin(yawRad));
            f = glm::normalize(f);
            // Right is orthogonal to Forward
            glm::vec3 r(-std::sin(yawRad), 0.0f, std::cos(yawRad));
            r = glm::normalize(r);

            bool moveForward = false;
            bool moveBackward = false;
            bool moveLeft = false;
            bool moveRight = false;
            bool sprint = false;
            bool jump = false;

            if (input) {
                moveForward = input->IsActionHeld("MoveUp");
                moveBackward = input->IsActionHeld("MoveDown");
                moveLeft = input->IsActionHeld("MoveLeft");
                moveRight = input->IsActionHeld("MoveRight");
                sprint = input->IsActionHeld("Sprint");
                jump = input->IsActionPressed("Jump");
            }
#ifdef _WIN32
            if (!moveForward) moveForward = (GetAsyncKeyState('W') & 0x8000) != 0;
            if (!moveBackward) moveBackward = (GetAsyncKeyState('S') & 0x8000) != 0;
            if (!moveLeft) moveLeft = (GetAsyncKeyState('A') & 0x8000) != 0;
            if (!moveRight) moveRight = (GetAsyncKeyState('D') & 0x8000) != 0;
            if (!sprint) sprint = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            if (!jump) jump = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
#endif
            if (ImGui::GetCurrentContext() != nullptr) {
                if (!moveForward && ImGui::IsKeyDown(ImGuiKey_W)) moveForward = true;
                if (!moveBackward && ImGui::IsKeyDown(ImGuiKey_S)) moveBackward = true;
                if (!moveLeft && ImGui::IsKeyDown(ImGuiKey_A)) moveLeft = true;
                if (!moveRight && ImGui::IsKeyDown(ImGuiKey_D)) moveRight = true;
                if (!sprint && ImGui::IsKeyDown(ImGuiKey_LeftShift)) sprint = true;
                if (!jump && ImGui::IsKeyDown(ImGuiKey_Space)) jump = true;
            }

            glm::vec3 inputDir(0.0f);
            if (moveForward) inputDir += f;
            if (moveBackward) inputDir -= f;
            if (moveLeft) inputDir -= r;
            if (moveRight) inputDir += r;

            if (glm::length(inputDir) > 0.001f) {
                inputDir = glm::normalize(inputDir);
            }

            float speed = sprint ? ccc.sprintSpeed : ccc.moveSpeed;
            glm::vec3 desiredHorizontalMove = inputDir * speed * fixedDeltaTime;

            // 2. Perform ground check using downward raycast from center
            bool isGrounded = false;
            float checkDist = ccc.capsuleHeight * 0.5f + ccc.groundCheckDistance;
            Vector3 groundRayOrigin = { transform.position.x, transform.position.y + ccc.capsuleHeight * 0.5f, transform.position.z };
            Vector3 rayDir = { 0.0f, -1.0f, 0.0f };
            eng::physics::RaycastHit groundHit;

            if (physicsWorld && physicsWorld->Raycast(groundRayOrigin, rayDir, checkDist, groundHit)) {
                if (ccc.velocity.y <= 0.0f) {
                    isGrounded = true;
                    ccc.velocity.y = 0.0f;
                }
            }

            // 3. Jump logic
            if (ccc.enableJump && isGrounded && jump) {
                ccc.velocity.y = ccc.jumpVelocity;
                isGrounded = false;
            }


            // 4. Gravity application
            if (!isGrounded) {
                ccc.velocity.y += ccc.gravity * fixedDeltaTime;
            }
            ccc.isGrounded = isGrounded;

            // 5. Try horizontal proposed move and resolve collision using capsule overlap sweep
            glm::vec3 currentPos(transform.position.x, transform.position.y, transform.position.z);
            glm::vec3 proposedPos = currentPos + desiredHorizontalMove;
            m_Blocked = false;

            if (desiredHorizontalMove.x != 0.0f || desiredHorizontalMove.z != 0.0f) {
                Vector3 capsuleCenter = { proposedPos.x, proposedPos.y + ccc.capsuleHeight * 0.5f, proposedPos.z };
                if (OverlapCapsuleFiltered(physicsWorld, capsuleCenter, ccc.capsuleRadius, ccc.capsuleHeight, m_PlayerEntity)) {
                    // Slide check: Try X movement only
                    glm::vec3 proposedX = currentPos + glm::vec3(desiredHorizontalMove.x, 0.0f, 0.0f);
                    Vector3 capsuleCenterX = { proposedX.x, proposedX.y + ccc.capsuleHeight * 0.5f, proposedX.z };
                    if (!OverlapCapsuleFiltered(physicsWorld, capsuleCenterX, ccc.capsuleRadius, ccc.capsuleHeight, m_PlayerEntity)) {
                        proposedPos = proposedX;
                    } else {
                        // Try Z movement only
                        glm::vec3 proposedZ = currentPos + glm::vec3(0.0f, 0.0f, desiredHorizontalMove.z);
                        Vector3 capsuleCenterZ = { proposedZ.x, proposedZ.y + ccc.capsuleHeight * 0.5f, proposedZ.z };
                        if (!OverlapCapsuleFiltered(physicsWorld, capsuleCenterZ, ccc.capsuleRadius, ccc.capsuleHeight, m_PlayerEntity)) {
                            proposedPos = proposedZ;
                        } else {
                            // Blocked completely on X & Z
                            proposedPos = currentPos;
                            m_Blocked = true;
                        }
                    }
                }
            }

            // 6. Apply vertical gravity movement
            proposedPos.y += ccc.velocity.y * fixedDeltaTime;

            // Optional downward grounding check at final Y position to prevent falling through floor
            if (!isGrounded && ccc.velocity.y <= 0.0f) {
                Vector3 finalGroundRayOrigin = { proposedPos.x, proposedPos.y + ccc.capsuleHeight * 0.5f, proposedPos.z };
                if (physicsWorld && physicsWorld->Raycast(finalGroundRayOrigin, rayDir, checkDist, groundHit)) {
                    proposedPos.y = groundHit.position.y;
                    ccc.velocity.y = 0.0f;
                    ccc.isGrounded = true;
                }
            }

            // Update transform coordinates
            transform.position.x = proposedPos.x;
            transform.position.y = proposedPos.y;
            transform.position.z = proposedPos.z;
        }

        void UpdateCameraLook(Coordinator& coordinator, bool hasFocus, InputManager* input = nullptr) {
            if (m_PlayerEntity == 0 || !coordinator.IsEntityAlive(m_PlayerEntity)) return;

            auto& ccc = coordinator.GetComponent<CharacterControllerComponent>(m_PlayerEntity);

            // Only capture mouse movements when cursor is confined/disabled in Play mode
            if (hasFocus) {
                float deltaX = 0.0f;
                float deltaY = 0.0f;

                if (input) {
                    auto [dx, dy] = input->GetMouseDelta();
                    deltaX = dx;
                    deltaY = dy;
                }

                if (deltaX == 0.0f && deltaY == 0.0f && ImGui::GetCurrentContext() != nullptr) {
                    ImGuiIO& io = ImGui::GetIO();
                    deltaX = io.MouseDelta.x;
                    deltaY = io.MouseDelta.y;
                }

                ccc.yaw += deltaX * ccc.mouseSensitivity;
                ccc.pitch -= deltaY * ccc.mouseSensitivity;
                ccc.pitch = std::clamp(ccc.pitch, -89.0f, 89.0f);
            }
        }


        bool IsBlocked() const { return m_Blocked; }

        std::shared_ptr<System> Clone() const override {
            auto clone = std::make_shared<PlayerControllerSystem>();
            clone->m_Entities = this->m_Entities;
            clone->m_PlayerEntity = this->m_PlayerEntity;
            clone->m_Blocked = this->m_Blocked;
            return clone;
        }

    private:
        Entity m_PlayerEntity = 0;
        bool m_Blocked = false;
    };

} // namespace eng::runtime
