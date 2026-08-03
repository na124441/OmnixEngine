#include "Gameplay/Validation/GameplayValidator.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include <iostream>

namespace eng::runtime {

    std::vector<ValidationResult> GameplayValidator::ValidateScene(Scene& scene) {
        std::vector<ValidationResult> results;

        // Track state variables
        bool foundPlayerStart = false;
        bool objectivesExist = false;
        bool activeObjectiveExists = false;

        std::unordered_set<std::string> objectiveIDs;
        std::unordered_set<std::string> checkpointIDs;
        std::unordered_set<std::string> activationIDs;

        struct TargetActivationCheck {
            Entity entity;
            std::string targetID;
        };
        std::vector<TargetActivationCheck> targetChecks;

        const auto& objects = scene.GetAllSceneObjects();

        // Step 1: Scan all SceneObjects for validations
        for (const auto& obj : objects) {
            if (!obj) continue;
            Entity entity = obj->GetECSEntity();

            // 1. PlayerStart validation
            if (obj->m_HasPlayerStart) {
                foundPlayerStart = true;
            }

            // 2. Objective validation
            if (obj->m_HasObjective) {
                objectivesExist = true;
                const auto& oc = obj->m_Objective;
                
                if (oc.ObjectiveID.empty()) {
                    results.push_back({
                        ValidationSeverity::Fatal,
                        "Objective ID is empty on object '" + obj->GetName() + "'",
                        entity,
                        "ObjectiveComponent"
                    });
                } else {
                    if (objectiveIDs.count(oc.ObjectiveID) > 0) {
                        results.push_back({
                            ValidationSeverity::Fatal,
                            "Duplicate Objective ID: '" + oc.ObjectiveID + "'",
                            entity,
                            "ObjectiveComponent"
                        });
                    } else {
                        objectiveIDs.insert(oc.ObjectiveID);
                    }
                }

                if (oc.StartsActive) {
                    activeObjectiveExists = true;
                }

                if (oc.CompletionMode == ObjectiveCompletionMode::TriggerEnter && !obj->m_HasTrigger) {
                    results.push_back({
                        ValidationSeverity::Error,
                        "Objective '" + oc.ObjectiveID + "' requires TriggerEnter but entity has no trigger component",
                        entity,
                        "ObjectiveComponent"
                    });
                }

                if (oc.CompletionMode == ObjectiveCompletionMode::Interaction && !obj->m_HasInteractable) {
                    results.push_back({
                        ValidationSeverity::Error,
                        "Objective '" + oc.ObjectiveID + "' requires Interaction but entity has no InteractableComponent",
                        entity,
                        "ObjectiveComponent"
                    });
                }
            }

            // 3. Interactable validation
            if (obj->m_HasInteractable) {
                const auto& ic = obj->m_Interactable;
                if (ic.PromptText.empty()) {
                    results.push_back({
                        ValidationSeverity::Warning,
                        "Interactable entity has empty prompt text",
                        entity,
                        "InteractableComponent"
                    });
                }
                if (ic.InteractionRadius <= 0.0f) {
                    results.push_back({
                        ValidationSeverity::Error,
                        "Interactable interaction radius must be greater than 0",
                        entity,
                        "InteractableComponent"
                    });
                }
                if (!ic.Enabled) {
                    results.push_back({
                        ValidationSeverity::Warning,
                        "Interactable component is disabled by default",
                        entity,
                        "InteractableComponent"
                    });
                }
            }

            // 4. AudioSource validation
            if (obj->m_HasAudioSource) {
                const auto& ac = obj->m_AudioSource;
                if (ac.ClipPath.empty()) {
                    results.push_back({
                        ValidationSeverity::Warning,
                        "Audio source has empty clip path",
                        entity,
                        "AudioSourceComponent"
                    });
                } else {
                    if (!std::filesystem::exists(ac.ClipPath)) {
                        results.push_back({
                            ValidationSeverity::Warning,
                            "Audio file does not exist: '" + ac.ClipPath + "'",
                            entity,
                            "AudioSourceComponent"
                        });
                    } else {
                        // Check extensions
                        std::string ext = std::filesystem::path(ac.ClipPath).extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext != ".wav" && ext != ".ogg" && ext != ".mp3") {
                            results.push_back({
                                ValidationSeverity::Warning,
                                "Unsupported audio extension '" + ext + "' (wav, ogg, mp3 recommended)",
                                entity,
                                "AudioSourceComponent"
                            });
                        }
                    }
                }
                if (ac.Volume < 0.0f || ac.Volume > 1.0f) {
                    results.push_back({
                        ValidationSeverity::Error,
                        "Audio volume must be between 0.0f and 1.0f",
                        entity,
                        "AudioSourceComponent"
                    });
                }
                if (ac.PlayOnStart && ac.ClipPath.empty()) {
                    results.push_back({
                        ValidationSeverity::Warning,
                        "Play-on-start audio has empty clip path",
                        entity,
                        "AudioSourceComponent"
                    });
                }
            }

            // 5. Checkpoint validation
            if (obj->m_HasCheckpoint) {
                const auto& cc = obj->m_Checkpoint;
                if (cc.CheckpointID.empty()) {
                    results.push_back({
                        ValidationSeverity::Fatal,
                        "Checkpoint ID is empty on object '" + obj->GetName() + "'",
                        entity,
                        "CheckpointComponent"
                    });
                } else {
                    if (checkpointIDs.count(cc.CheckpointID) > 0) {
                        results.push_back({
                            ValidationSeverity::Error,
                            "Duplicate Checkpoint ID: '" + cc.CheckpointID + "'",
                            entity,
                            "CheckpointComponent"
                        });
                    } else {
                        checkpointIDs.insert(cc.CheckpointID);
                    }
                }
                if (cc.CheckpointName.empty()) {
                    results.push_back({
                        ValidationSeverity::Warning,
                        "Checkpoint name is empty",
                        entity,
                        "CheckpointComponent"
                    });
                }
                if (cc.ActivateOnTriggerEnter && !obj->m_HasTrigger) {
                    results.push_back({
                        ValidationSeverity::Error,
                        "Trigger-based checkpoint '" + cc.CheckpointID + "' has no trigger volume",
                        entity,
                        "CheckpointComponent"
                    });
                }
            }

            // 6. Activatable validation
            if (obj->m_HasActivatable) {
                const auto& ac = obj->m_Activatable;
                if (!ac.ActivationID.empty()) {
                    if (activationIDs.count(ac.ActivationID) > 0) {
                        results.push_back({
                            ValidationSeverity::Warning,
                            "Duplicate Activation ID: '" + ac.ActivationID + "'",
                            entity,
                            "ActivatableComponent"
                        });
                    } else {
                        activationIDs.insert(ac.ActivationID);
                    }
                }
                if (!ac.TargetActivationID.empty()) {
                    targetChecks.push_back({entity, ac.TargetActivationID});
                    if (ac.TargetActivationID == ac.ActivationID) {
                        results.push_back({
                            ValidationSeverity::Warning,
                            "Activatable object targets itself: '" + ac.ActivationID + "'",
                            entity,
                            "ActivatableComponent"
                        });
                    }
                    if (ac.OneShot && ac.HasActivated) {
                        results.push_back({
                            ValidationSeverity::Warning,
                            "One-shot activatable starts as already activated in authoring data",
                            entity,
                            "ActivatableComponent"
                        });
                    }
                } else {
                    // It's a button or trigger targeting nothing
                    if (obj->m_HasInteractable && ac.ActivationID.empty()) {
                        results.push_back({
                            ValidationSeverity::Warning,
                            "Terminal/interactable activatable has empty target activation ID",
                            entity,
                            "ActivatableComponent"
                        });
                    }
                }
            }

            // 7. Door validation
            if (obj->m_HasDoor) {
                const auto& dc = obj->m_Door;
                if (!obj->m_HasSimpleState) {
                    results.push_back({
                        ValidationSeverity::Error,
                        "DoorComponent exists, but SimpleStateComponent is missing",
                        entity,
                        "DoorComponent"
                    });
                }
                if (dc.OpenOffset.x == 0.0f && dc.OpenOffset.y == 0.0f && dc.OpenOffset.z == 0.0f) {
                    results.push_back({
                        ValidationSeverity::Warning,
                        "Door has zero open offset",
                        entity,
                        "DoorComponent"
                    });
                }
                if (dc.OpenMode == DoorOpenMode::Smooth && dc.OpenSpeed <= 0.0f) {
                    results.push_back({
                        ValidationSeverity::Error,
                        "DoorComponent smooth mode has OpenSpeed = 0",
                        entity,
                        "DoorComponent"
                    });
                }
            }
        }

        // Step 2: Perform global checks
        
        // A. GameMode check (Info)
        results.push_back({
            ValidationSeverity::Info,
            "GameMode: Default VerticalSliceGameMode will be assigned",
            INVALID_ENTITY,
            "GameMode"
        });

        // B. PlayerStart existence check
        if (!foundPlayerStart) {
            results.push_back({
                ValidationSeverity::Fatal,
                "No PlayerStart exists in the scene",
                INVALID_ENTITY,
                "PlayerStart"
            });
        }

        // C. Active objective check
        if (objectivesExist && !activeObjectiveExists) {
            results.push_back({
                ValidationSeverity::Error,
                "Objectives exist but none starts active",
                INVALID_ENTITY,
                "ObjectiveComponent"
            });
        }

        // D. Verify activation chain targets exist in scene
        for (const auto& check : targetChecks) {
            if (activationIDs.count(check.targetID) == 0) {
                results.push_back({
                    ValidationSeverity::Error,
                    "Entity targets activation ID '" + check.targetID + "', but no object owns that ID",
                    check.entity,
                    "ActivatableComponent"
                });
            }
        }

        return results;
    }

    bool GameplayValidator::HasFatalErrors(const std::vector<ValidationResult>& results) const {
        for (const auto& res : results) {
            if (res.Severity == ValidationSeverity::Fatal) {
                return true;
            }
        }
        return false;
    }

} // namespace eng::runtime
