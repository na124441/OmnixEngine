//============================================================================
// PlayerSystem.h - Player Control System
//
// Handles player movement and input
// Requires: TransformComponent + RigidBodyComponent + PlayerControllerComponent
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include "SystemManager.h"
#include "ECSComponents.h"
#include "Coordinator.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"
#include "../Input/InputManager.h"
#include <iostream>

/**
 * @brief PlayerSystem - Handles player control
 */
class PlayerSystem : public System {
public:
    /**
     * @brief Update player entities
     * @param deltaTime Time step
     * @param coordinator ECS coordinator
     * @param inputManager Input manager for reading user input
     */
    void Update(float deltaTime, Coordinator& coordinator, InputManager* inputManager) {
        for (Entity entity : m_Entities) {
            // Check if ZoneEntityComponent is attached and simulating is false
            auto signature = coordinator.GetSignature(entity);
            if (signature.test(coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>())) {
                const auto& zec = coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entity);
                if (!zec.simulating) {
                    continue;
                }
            }

            auto& transform = coordinator.GetComponent<TransformComponent>(entity);
            auto& rigidBody = coordinator.GetComponent<RigidBodyComponent>(entity);
            auto& player = coordinator.GetComponent<PlayerControllerComponent>(entity);

            Vector3 moveDir(0, 0, 0);

            if (inputManager) {
                if (inputManager->IsActionHeld("MoveForward")) moveDir.z += 1.0f;
                if (inputManager->IsActionHeld("MoveBackward")) moveDir.z -= 1.0f;
                if (inputManager->IsActionHeld("MoveLeft")) moveDir.x -= 1.0f;
                if (inputManager->IsActionHeld("MoveRight")) moveDir.x += 1.0f;
                
                if (inputManager->IsActionPressed("Jump") && std::abs(rigidBody.velocity.y) < 0.01f) {
                    rigidBody.velocity.y = 5.0f; // Simple jump impulse
                }
            }

            // Apply movement velocity
            if (moveDir.x != 0 || moveDir.z != 0) {
                // Normalize and apply speed
                float mag = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
                moveDir.x /= mag;
                moveDir.z /= mag;

                rigidBody.velocity.x = moveDir.x * player.moveSpeed;
                rigidBody.velocity.z = moveDir.z * player.moveSpeed;
            } else {
                // Apply braking/drag when no input
                rigidBody.velocity.x *= 0.9f;
                rigidBody.velocity.z *= 0.9f;
            }
        }
    }

    std::shared_ptr<System> Clone() const override {
        auto clone = std::make_shared<PlayerSystem>();
        clone->m_Entities = this->m_Entities;
        return clone;
    }
};
