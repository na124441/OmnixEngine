#include "Gameplay/Systems/InteractionSystem.h"
#include "Gameplay/PlayerStateComponent.h"

#include "Gameplay/GameplayEvent.h"
#include "Gameplay/GameplayEventBus.h"
#include "Input/InputManager.h"
#include "Physics/Public/PhysicsWorld.h"
#include "Physics/Public/PhysicsQueries.h"
#include "ECS/Public/IECSWorld.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace eng::runtime {

    void InteractionSystem::Update(float dt, RuntimeContext& context)
    {
        if (!context.ecs) return;

        bool shouldSimulate = (context.mode == RuntimeMode::Game) ||
                              (context.mode == RuntimeMode::Editor && context.editorSimulationState == EditorSimulationState::Play);
        if (!shouldSimulate) {
            context.interactionPrompt.Visible = false;
            context.interactionPrompt.Text = "";
            context.interactionPrompt.Type = InteractionType::None;
            context.interactionPrompt.Target = INVALID_ENTITY;

            auto& coordinator = context.ecs->getCoordinator();
            auto playerStateType = coordinator.GetComponentType<PlayerStateComponent>();
            for (Entity entity : coordinator.GetActiveEntities()) {
                if (entity != INVALID_ENTITY && coordinator.IsEntityAlive(entity)) {
                    auto sig = coordinator.GetSignature(entity);
                    if (sig.test(playerStateType)) {
                        auto& psc = coordinator.GetComponent<PlayerStateComponent>(entity);
                        psc.CurrentInteractionTarget = INVALID_ENTITY;
                        break;
                    }
                }
            }
            return;
        }

        auto& coordinator = context.ecs->getCoordinator();

        // Find player entity with PlayerStateComponent
        Entity playerEnt = INVALID_ENTITY;
        auto playerStateType = coordinator.GetComponentType<PlayerStateComponent>();

        for (Entity entity : coordinator.GetActiveEntities()) {
            if (entity != INVALID_ENTITY && coordinator.IsEntityAlive(entity)) {
                auto sig = coordinator.GetSignature(entity);
                if (sig.test(playerStateType)) {
                    playerEnt = entity;
                    break;
                }
            }
        }

        if (playerEnt == INVALID_ENTITY || !coordinator.IsEntityAlive(playerEnt)) {
            // Clear prompt and return
            context.interactionPrompt.Visible = false;
            context.interactionPrompt.Text = "";
            context.interactionPrompt.Type = InteractionType::None;
            context.interactionPrompt.Target = INVALID_ENTITY;
            return;
        }

        auto& psc = coordinator.GetComponent<PlayerStateComponent>(playerEnt);

        // Get player position and forward look direction
        auto transformType = coordinator.GetComponentType<TransformComponent>();
        if (!coordinator.GetSignature(playerEnt).test(transformType)) {
            return;
        }

        const auto& transform = coordinator.GetComponent<TransformComponent>(playerEnt);
        glm::vec3 eyePos(transform.position.x, transform.position.y, transform.position.z);
        glm::vec3 forwardDir(1.0f, 0.0f, 0.0f); // Default forward

        auto cccType = coordinator.GetComponentType<CharacterControllerComponent>();
        auto camType = coordinator.GetComponentType<CameraComponent>();
        auto sig = coordinator.GetSignature(playerEnt);

        if (sig.test(cccType)) {
            const auto& ccc = coordinator.GetComponent<CharacterControllerComponent>(playerEnt);
            float yawRad = glm::radians(ccc.yaw);
            float pitchRad = glm::radians(ccc.pitch);
            forwardDir = glm::vec3(
                std::cos(pitchRad) * std::cos(yawRad),
                std::sin(pitchRad),
                std::cos(pitchRad) * std::sin(yawRad)
            );
            forwardDir = glm::normalize(forwardDir);

            if (sig.test(camType)) {
                const auto& cameraComp = coordinator.GetComponent<CameraComponent>(playerEnt);
                glm::vec3 rightDir = glm::normalize(glm::vec3(-std::sin(yawRad), 0.0f, std::cos(yawRad)));
                glm::vec3 forwardDirXZ = glm::normalize(glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad)));
                glm::vec3 rotatedOffset = 
                    cameraComp.localOffset.x * rightDir + 
                    cameraComp.localOffset.y * glm::vec3(0.0f, 1.0f, 0.0f) + 
                    cameraComp.localOffset.z * forwardDirXZ;
                eyePos += rotatedOffset;
            } else {
                eyePos.y += ccc.capsuleHeight * 0.5f;
            }
        }

        Entity bestTarget = INVALID_ENTITY;

        // 1. Raycast priority check
        if (context.physicsWorld) {
            eng::physics::RaycastHit hit;
            Vector3 origin = { eyePos.x, eyePos.y, eyePos.z };
            Vector3 direction = { forwardDir.x, forwardDir.y, forwardDir.z };
            if (context.physicsWorld->Raycast(origin, direction, 10.0f, hit)) {
                if (hit.entity != INVALID_ENTITY && coordinator.IsEntityAlive(hit.entity)) {
                    auto entitySig = coordinator.GetSignature(hit.entity);
                    auto interactableType = coordinator.GetComponentType<InteractableComponent>();
                    if (entitySig.test(interactableType)) {
                        const auto& interactable = coordinator.GetComponent<InteractableComponent>(hit.entity);
                        if (interactable.Enabled) {
                            if (hit.distance <= interactable.InteractionRadius) {
                                bestTarget = hit.entity;
                            }
                        }
                    }
                }
            }
        }

        // 2. Hybrid fallback (dot product / distance inside radius)
        if (bestTarget == INVALID_ENTITY) {
            float nearestDistSq = FLT_MAX;
            float bestDot = -1.0f;
            Entity nearestEntity = INVALID_ENTITY;
            Entity bestDotEntity = INVALID_ENTITY;

            auto interactableType = coordinator.GetComponentType<InteractableComponent>();

            for (Entity entity : m_Entities) {
                if (entity == playerEnt) continue;
                if (!coordinator.IsEntityAlive(entity)) continue;

                const auto& interactable = coordinator.GetComponent<InteractableComponent>(entity);
                if (!interactable.Enabled) continue;

                const auto& trans = coordinator.GetComponent<TransformComponent>(entity);
                glm::vec3 targetPos(trans.position.x, trans.position.y, trans.position.z);
                glm::vec3 toTarget = targetPos - eyePos;
                float distSq = glm::dot(toTarget, toTarget);
                float dist = std::sqrt(distSq);

                if (dist <= interactable.InteractionRadius) {
                    glm::vec3 dirToTarget = (dist > 0.0001f) ? (toTarget / dist) : glm::vec3(0.0f);
                    float dot = glm::dot(forwardDir, dirToTarget);

                    if (distSq < nearestDistSq) {
                        nearestDistSq = distSq;
                        nearestEntity = entity;
                    }

                    // Prefer targets in front of player (dot > 0.5f)
                    if (dot > 0.5f) {
                        if (dot > bestDot) {
                            bestDot = dot;
                            bestDotEntity = entity;
                        }
                    }
                }
            }

            if (bestDotEntity != INVALID_ENTITY) {
                bestTarget = bestDotEntity;
            } else {
                bestTarget = nearestEntity;
            }
        }

        // 3. Update player state & context interaction prompt
        psc.CurrentInteractionTarget = bestTarget;

        if (bestTarget != INVALID_ENTITY && coordinator.IsEntityAlive(bestTarget)) {
            const auto& targetInteractable = coordinator.GetComponent<InteractableComponent>(bestTarget);
            context.interactionPrompt.Visible = true;
            context.interactionPrompt.Text = targetInteractable.PromptText;
            context.interactionPrompt.Type = targetInteractable.Type;
            context.interactionPrompt.Target = bestTarget;

            // Check input for E key press
            if (context.input && context.input->IsActionPressed("Interact")) {
                if (context.gameplayEventBus) {
                    GameplayEvent event;
                    event.Type = GameplayEventType::Interaction;
                    event.Source = playerEnt;
                    event.Target = bestTarget;
                    context.gameplayEventBus->QueueEvent(event);
                }
            }
        } else {
            context.interactionPrompt.Visible = false;
            context.interactionPrompt.Text = "";
            context.interactionPrompt.Type = InteractionType::None;
            context.interactionPrompt.Target = INVALID_ENTITY;
        }
    }

} // namespace eng::runtime
