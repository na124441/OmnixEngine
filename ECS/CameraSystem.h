//============================================================================
// CameraSystem.h - Camera Management System
//
// Updates camera view and projection matrices
// Requires: TransformComponent + CameraComponent
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include "SystemManager.h"
#include "ECSComponents.h"
#include "Coordinator.h"
#include <iostream>

/**
 * @brief CameraSystem - Handles camera updates
 */
class CameraSystem : public System {
public:
    /**
     * @brief Update camera entities
     * @param deltaTime Time step
     * @param coordinator ECS coordinator
     */
    void Update(float deltaTime, Coordinator& coordinator) {
        for (Entity entity : m_Entities) {
            auto& transform = coordinator.GetComponent<TransformComponent>(entity);
            auto& camera = coordinator.GetComponent<CameraComponent>(entity);

            if (camera.isPrimary) {
                // Update primary camera view matrix
                // ViewMatrix = Inverse(TransformMatrix)
                // TODO: Implement Matrix4x4 inverse and look-at
                
                // For now, just log the camera position
                /*
                std::cout << "[CameraSystem] Primary Camera at (" 
                          << transform.position.x << ", " 
                          << transform.position.y << ", " 
                          << transform.position.z << ")" << std::endl;
                */
            }
        }
    }

    std::shared_ptr<System> Clone() const override {
        auto clone = std::make_shared<CameraSystem>();
        clone->m_Entities = this->m_Entities;
        return clone;
    }
};
