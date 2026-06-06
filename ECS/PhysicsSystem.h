//============================================================================
// PhysicsSystem.h - Physics Simulation System
//
// Updates RigidBody components with physics simulation
// Requires: TransformComponent + RigidBodyComponent
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include "SystemManager.h"
#include "ECSConfig.h"
#include "ECSComponents.h"
#include "Coordinator.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"
#include <iostream>

/**
 * @brief PhysicsSystem - Handles physics simulation
 *
 * System Signature:
 * - TransformComponent (required)
 * - RigidBodyComponent (required)
 * - ColliderComponent (optional, for collision)
 */
class PhysicsSystem : public System {
public:
    /**
     * @brief Update physics simulation
     * @param deltaTime Time step
     * @param coordinator ECS coordinator
     */
    void Update(float deltaTime, Coordinator& coordinator) {
        std::cout << "[PhysicsSystem] Updating " << m_Entities.size()
                  << " entities..." << std::endl;

        for (Entity entity : m_Entities) {
            // Check if ZoneEntityComponent is attached and simulating is false
            auto signature = coordinator.GetSignature(entity);
            if (signature.test(coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>())) {
                const auto& zec = coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entity);
                if (!zec.simulating) {
                    continue;
                }
            }

            // Get components
            auto& transform = coordinator.GetComponent<TransformComponent>(entity);
            auto& rigidBody = coordinator.GetComponent<RigidBodyComponent>(entity);

            // Skip kinematic bodies
            if (rigidBody.isKinematic) {
                continue;
            }

            // Apply gravity
            if (rigidBody.useGravity) {
                rigidBody.velocity.y += -9.81f * deltaTime;
            }

            // Apply drag
            if (rigidBody.drag > 0.0f) {
                float dragFactor = 1.0f - (rigidBody.drag * deltaTime);
                if (dragFactor < 0.0f) dragFactor = 0.0f;

                rigidBody.velocity.x *= dragFactor;
                rigidBody.velocity.y *= dragFactor;
                rigidBody.velocity.z *= dragFactor;
            }

            // Update position (integrate velocity)
            if (!rigidBody.freezePositionX) {
                transform.position.x += rigidBody.velocity.x * deltaTime;
            }
            if (!rigidBody.freezePositionY) {
                transform.position.y += rigidBody.velocity.y * deltaTime;
            }
            if (!rigidBody.freezePositionZ) {
                transform.position.z += rigidBody.velocity.z * deltaTime;
            }

            // Update rotation (integrate angular velocity)
            if (!rigidBody.freezeRotationX && !rigidBody.freezeRotationY && !rigidBody.freezeRotationZ) {
                // Simplified rotation integration
                // TODO: Proper quaternion integration
            }

            // Mark transform as dirty
            transform.dirty = true;
        }
    }

    /**
     * @brief Apply force to entity
     * @param entity Entity to apply force to
     * @param force Force vector
     * @param coordinator ECS coordinator
     */
    void ApplyForce(Entity entity, const Vector3& force, Coordinator& coordinator) {
        auto& rigidBody = coordinator.GetComponent<RigidBodyComponent>(entity);

        // F = ma → a = F/m
        Vector3 acceleration = force;
        if (rigidBody.mass > 0.0f) {
            acceleration.x /= rigidBody.mass;
            acceleration.y /= rigidBody.mass;
            acceleration.z /= rigidBody.mass;
        }

        rigidBody.velocity.x += acceleration.x;
        rigidBody.velocity.y += acceleration.y;
        rigidBody.velocity.z += acceleration.z;
    }

    /**
     * @brief Apply impulse to entity (instant velocity change)
     * @param entity Entity to apply impulse to
     * @param impulse Impulse vector
     * @param coordinator ECS coordinator
     */
    void ApplyImpulse(Entity entity, const Vector3& impulse, Coordinator& coordinator) {
        auto& rigidBody = coordinator.GetComponent<RigidBodyComponent>(entity);

        rigidBody.velocity.x += impulse.x;
        rigidBody.velocity.y += impulse.y;
        rigidBody.velocity.z += impulse.z;
    }

    std::shared_ptr<System> Clone() const override {
        auto clone = std::make_shared<PhysicsSystem>();
        clone->m_Entities = this->m_Entities;
        return clone;
    }
};

//============================================================================
// END OF FILE
//===============================================================