//============================================================================
// RenderSystem.h - Rendering System
//
// Submits render commands for entities with MeshRenderer
// Requires: TransformComponent + MeshRendererComponent
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include "SystemManager.h"
#include "ECSConfig.h"
#include "ECSComponents.h"
#include "Coordinator.h"
#include "../Scene/Matrix4x4.h"
#include <iostream>
#include <vector>
#include <string>

/**
 * @brief RenderCommand - Single render submission
 */
struct RenderCommand {
    uint32_t meshID;
    uint32_t materialID;
    Matrix4x4 worldMatrix;
    bool castShadows;
    bool receiveShadows;
};

/**
 * @brief RenderSystem - Handles mesh rendering
 *
 * System Signature:
 * - TransformComponent (required)
 * - MeshRendererComponent (required)
 */
class RenderSystem : public System {
public:
    /**
     * @brief Update render system
     * @param deltaTime Time step
     * @param coordinator ECS coordinator
     */
    void Update(float deltaTime, Coordinator& coordinator) {
        // Clear render queue
        m_RenderCommands.clear();

        for (Entity entity : m_Entities) {
            // Get components
            auto& transform = coordinator.GetComponent<TransformComponent>(entity);
            auto& renderer = coordinator.GetComponent<MeshRendererComponent>(entity);

            // Skip invisible objects
            if (!renderer.visible) {
                continue;
            }

            // Update world matrix if dirty
            if (transform.dirty) {
                UpdateWorldMatrix(transform);
                transform.dirty = false;
            }

            // Create render command
            RenderCommand cmd;
            cmd.meshID = renderer.meshID;
            cmd.materialID = renderer.materialID;
            cmd.worldMatrix = transform.worldMatrix;
            cmd.castShadows = renderer.castShadows;
            cmd.receiveShadows = renderer.receiveShadows;

            // Add to render queue
            m_RenderCommands.push_back(cmd);
        }
    }

    /**
     * @brief Get render queue
     * @return Const reference to render commands
     */
    const std::vector<RenderCommand>& GetRenderCommands() const {
        return m_RenderCommands;
    }

    std::shared_ptr<System> Clone() const override {
        auto clone = std::make_shared<RenderSystem>();
        clone->m_Entities = this->m_Entities;
        clone->m_RenderCommands = this->m_RenderCommands;
        return clone;
    }

private:
    std::vector<RenderCommand> m_RenderCommands;

    /**
     * @brief Update world matrix from transform
     * @param transform Transform component
     */
    void UpdateWorldMatrix(TransformComponent& transform) {
        // Use the Matrix4x4 utility to compute the TRS matrix
        transform.worldMatrix = Matrix4x4::TRS(transform.position, transform.rotation, transform.scale);
    }
};