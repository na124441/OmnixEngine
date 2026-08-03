#include "Core/Diagnostics/StressTest.h"
#include "imgui.h"
#include "Core/World.h"
#include "Systems/Scheduler/SystemScheduler.h"
#include "Serializer/ECS/ECS.h"
#include "Serializer/ECS/SchemaRegistry.h"
#include "Serializer/ECS/SerializationBridge.h"
#include "Serializer/Serialization/Normal/NormalSerializer.h"
#include "Serializer/Serialization/Normal/NormalDeserializer.h"
#include "Core/Memory/AllocationTracker.h"
#include "Core/Logger.h"
#include "Components/Logical/Health.h"
#include "Components/Spatial/Transform.h"
#include "Components/Physical/RigidBody.h"
#include "Gameplay/VerticalSliceGameMode.h"
#include "Gameplay/PlayerStateComponent.h"
#include "Gameplay/Components/InteractableComponent.h"
#define Transform SceneTransform
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneLoader.h"
#include "Scene/SceneObject.h"
#undef Transform
#include "Gameplay/Validation/GameplayValidator.h"
#include "Gameplay/Systems/InteractionSystem.h"
#include "Gameplay/Components/ObjectiveComponent.h"
#include "Gameplay/Objectives/ObjectiveSystem.h"
#include "Gameplay/UI/GameplayHUD.h"
#include "Input/InputManager.h"
#include "Runtime/RuntimeContext.h"
#include "Gameplay/GameState.h"
#include "Gameplay/GameplayEvent.h"
#include "Gameplay/GameplayEventBus.h"
#include "Runtime/Audio/AudioSystem.h"
#include "Gameplay/StateObjects/SimpleStateComponent.h"
#include "Gameplay/StateObjects/ActivatableComponent.h"
#include "Gameplay/StateObjects/DoorComponent.h"
#include "Gameplay/StateObjects/ObjectActivationSystem.h"
#include "Gameplay/Checkpoints/CheckpointComponent.h"
#include "Gameplay/Checkpoints/CheckpointSnapshot.h"
#include "Gameplay/CheckpointSystem.h"
#include "Gameplay/Save/GameplaySaveSystem.h"
#include "Gameplay/Save/GameplaySaveSnapshot.h"
#include "Gameplay/Save/GameplaySaveHeader.h"
#include <atomic>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>

namespace eng::diagnostics {

    bool RunRuntimeStressTests() {
        LOG_INFO("================================================================================");
        LOG_INFO("                  RUNNING RUNTIME INTEGRATION STRESS TESTS                      ");
        LOG_INFO("================================================================================");

        size_t initialAllocations = eng::memory::AllocationTracker::GetActiveAllocationsCount();
        LOG_INFO("[Stress] Initial active tracked memory allocations: %zu", initialAllocations);

        // -----------------------------------------------------------------------------
        // 1. ECS STRESS TEST (100K Entities)
        // -----------------------------------------------------------------------------
        LOG_INFO("[Stress] Starting ECS Stress Test with 100K entities...");
        {
            auto world = std::make_unique<eng::runtime::World>();
            world->Initialize();

            std::vector<::Entity> entities;
            entities.reserve(100000);

            // Spawning 100K entities
            for (int i = 0; i < 100000; ++i) {
                entities.push_back(world->CreateEntity());
            }

            // Attaching components
            for (auto entity : entities) {
                TransformComponent transform;
                transform.position = Vector3(1.0f, 2.0f, 3.0f);
                world->AddComponent<TransformComponent>(entity, transform);
            }

            // Tick the ECS world
            world->Update(0.016f);

            // Cleanup entities
            for (auto entity : entities) {
                world->DestroyEntity(entity);
            }

            world->Shutdown();
        }
        LOG_INFO("[Stress] ECS Stress Test passed successfully.");

        // -----------------------------------------------------------------------------
        // 2. SCHEDULER STRESS TEST (10K Tasks)
        // -----------------------------------------------------------------------------
        LOG_INFO("[Stress] Starting Scheduler Stress Test with 10K tasks...");
        {
            auto scheduler = std::make_unique<eng::runtime::SystemScheduler>();
            scheduler->Initialize();

            std::atomic<int> counter{0};
            for (int i = 0; i < 10000; ++i) {
                scheduler->Execute([&counter]() {
                    counter.fetch_add(1, std::memory_order_relaxed);
                });
            }

            // Run pending tasks
            scheduler->RunPending();

            if (counter.load() != 10000) {
                LOG_ERROR("[Stress] Scheduler Stress Test FAILED: expected 10000 executed tasks, got %d", counter.load());
                return false;
            }

            scheduler->Shutdown();
        }
        LOG_INFO("[Stress] Scheduler Stress Test passed successfully.");

        // -----------------------------------------------------------------------------
        // 3. SERIALIZATION STAGING (Schema, Bridge, Serializer, Deserializer)
        // -----------------------------------------------------------------------------
        LOG_INFO("[Stress] Starting Serialization Staging Integration Test...");
        {
            ComponentSchemaRegistry registry;

            // Register schemas
            static FieldSchema healthFields[] = {
                { "health", offsetof(Health, health), FieldType::FLOAT, INTENT_SERIALIZABLE, sizeof(float) },
                { "maxHealth", offsetof(Health, maxHealth), FieldType::FLOAT, INTENT_SERIALIZABLE, sizeof(float) },
                { "isDead", offsetof(Health, isDead), FieldType::BOOL, INTENT_SERIALIZABLE, sizeof(bool) }
            };
            ComponentSchema healthSchema = {
                "Health",
                sizeof(Health),
                healthFields,
                3
            };
            registry.RegisterSchema(HEALTH_COMPONENT, healthSchema);

            static FieldSchema transformFields[] = {
                { "position", offsetof(Transform, position), FieldType::VEC3, INTENT_SERIALIZABLE, sizeof(Vector3) },
                { "rotation", offsetof(Transform, rotation), FieldType::QUAT, INTENT_SERIALIZABLE, sizeof(Quaternion) },
                { "scale", offsetof(Transform, scale), FieldType::VEC3, INTENT_SERIALIZABLE, sizeof(Vector3) }
            };
            ComponentSchema transformSchema = {
                "Transform",
                sizeof(Transform),
                transformFields,
                3
            };
            registry.RegisterSchema(TRANSFORM_COMPONENT, transformSchema);

            static FieldSchema rigidbodyFields[] = {
                { "velocity", offsetof(RigidBody, velocity), FieldType::VEC3, INTENT_SERIALIZABLE, sizeof(Vector3) },
                { "mass", offsetof(RigidBody, mass), FieldType::FLOAT, INTENT_SERIALIZABLE, sizeof(float) }
            };
            ComponentSchema rigidbodySchema = {
                "RigidBody",
                sizeof(RigidBody),
                rigidbodyFields,
                2
            };
            registry.RegisterSchema(PHYSICS_COMPONENT, rigidbodySchema);

            // Instantiate ECS
            ECS ecs;
            ecs.Initialize(&registry);

            LOG_INFO("[Stress] Layout info - Transform Size: %zu", sizeof(Transform));
            LOG_INFO("[Stress] Layout info - position offset: %zu", offsetof(Transform, position));
            LOG_INFO("[Stress] Layout info - rotation offset: %zu", offsetof(Transform, rotation));
            LOG_INFO("[Stress] Layout info - scale offset: %zu", offsetof(Transform, scale));

            // Spawn entities with components
            uint32_t ent1 = ecs.CreateEntity();
            Health h1{ 50.0f, 100.0f, false };
            ecs.AddComponent(ent1, h1);
            Transform t1{ Vector3(1.0f, 2.0f, 3.0f), Quaternion(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f) };
            ecs.AddComponent(ent1, t1);

            uint32_t ent2 = ecs.CreateEntity();
            RigidBody rb2{ Vector3(0.0f, -9.8f, 0.0f), 10.0f };
            ecs.AddComponent(ent2, rb2);

            // Capture snapshot via bridge
            SerializationBridge bridge(ecs, registry);
            SnapshotContext context(SNAPSHOT_SAVE);
            bridge.Capture(context);

            const ECSSnapshot& origSnapshot = bridge.GetSnapshot();

            LOG_INFO("=== Original Snapshot Debug Summary ===");
            for (const auto& ent : origSnapshot.GetEntitySnapshots()) {
                LOG_INFO("Entity %u: component count %zu", ent.GetEntityID(), ent.GetComponentSnapshots().size());
                for (const auto& [compType, comp] : ent.GetComponentSnapshots()) {
                    LOG_INFO("  Component %u: fields %zu", compType, comp.GetFieldSnapshots().size());
                    for (const auto& f : comp.GetFieldSnapshots()) {
                        LOG_INFO("    Field %u: size %zu, data: %s", f.GetFieldID(), f.GetFieldData().Size(), FieldDataToString(f).c_str());
                    }
                }
            }

            // 3.1: Buffer Roundtrip
            NormalSerializer serializer;
            std::vector<uint8_t> buffer;
            if (!serializer.SerializeToBuffer(origSnapshot, buffer)) {
                LOG_ERROR("[Stress] Serialization to buffer FAILED: %s", serializer.GetLastError().c_str());
                return false;
            }

            NormalDeserializer deserializer;
            ECSSnapshot* loadedSnapshot = deserializer.DeserializeToSnapshot(buffer.data(), buffer.size(), registry);
            if (!loadedSnapshot) {
                LOG_ERROR("[Stress] Deserialization from buffer FAILED: %s", deserializer.GetLastError().c_str());
                return false;
            }

            // Verify equivalence
            auto verifyEquivalence = [](const ECSSnapshot& orig, const ECSSnapshot& loaded) -> bool {
                if (orig.GetEntitySnapshots().size() != loaded.GetEntitySnapshots().size()) {
                    LOG_ERROR("[Stress] Entity size mismatch: %zu vs %zu", orig.GetEntitySnapshots().size(), loaded.GetEntitySnapshots().size());
                    return false;
                }
                for (size_t i = 0; i < orig.GetEntitySnapshots().size(); ++i) {
                    const auto& origEnt = orig.GetEntitySnapshots()[i];
                    const auto& loadEnt = loaded.GetEntitySnapshots()[i];
                    if (origEnt.GetEntityID() != loadEnt.GetEntityID()) {
                        LOG_ERROR("[Stress] Entity ID mismatch at index %zu: %u vs %u", i, origEnt.GetEntityID(), loadEnt.GetEntityID());
                        return false;
                    }
                    if (origEnt.GetComponentSnapshots().size() != loadEnt.GetComponentSnapshots().size()) {
                        LOG_ERROR("[Stress] Component count mismatch for entity %u: %zu vs %zu", origEnt.GetEntityID(), origEnt.GetComponentSnapshots().size(), loadEnt.GetComponentSnapshots().size());
                        return false;
                    }
                    for (const auto& [compType, origComp] : origEnt.GetComponentSnapshots()) {
                        const auto* loadComp = loadEnt.GetComponentSnapshot(compType);
                        if (!loadComp) {
                            LOG_ERROR("[Stress] Component type %u missing in loaded snapshot for entity %u", compType, origEnt.GetEntityID());
                            return false;
                        }
                        if (origComp.GetFieldSnapshots().size() != loadComp->GetFieldSnapshots().size()) {
                            LOG_ERROR("[Stress] Field count mismatch for component %u on entity %u: %zu vs %zu", compType, origEnt.GetEntityID(), origComp.GetFieldSnapshots().size(), loadComp->GetFieldSnapshots().size());
                            return false;
                        }
                        for (size_t k = 0; k < origComp.GetFieldSnapshots().size(); ++k) {
                            const auto& origField = origComp.GetFieldSnapshots()[k];
                            const auto& loadField = loadComp->GetFieldSnapshots()[k];
                            if (origField.GetFieldID() != loadField.GetFieldID()) {
                                LOG_ERROR("[Stress] Field ID mismatch at index %zu in component %u on entity %u: %u vs %u", k, compType, origEnt.GetEntityID(), origField.GetFieldID(), loadField.GetFieldID());
                                return false;
                            }
                            if (!origField.GetFieldData().Equals(loadField.GetFieldData())) {
                                LOG_ERROR("[Stress] Field data mismatch for field %u in component %u on entity %u (Size: %zu vs %zu)", origField.GetFieldID(), compType, origEnt.GetEntityID(), origField.GetFieldData().Size(), loadField.GetFieldData().Size());
                                LOG_ERROR("[Stress] Original hex: %s", FieldDataToString(origField).c_str());
                                LOG_ERROR("[Stress] Loaded hex:   %s", FieldDataToString(loadField).c_str());
                                return false;
                            }
                        }
                    }
                }
                return true;
            };

            if (!verifyEquivalence(origSnapshot, *loadedSnapshot)) {
                LOG_ERROR("[Stress] Serialization buffer roundtrip verification FAILED!");
                delete loadedSnapshot;
                return false;
            }
            delete loadedSnapshot;

            // 3.2: File Roundtrip
            std::string filepath = "stress_test_temp.snapshot.bin";
            if (!serializer.SerializeToFile(origSnapshot, filepath)) {
                LOG_ERROR("[Stress] Serialization to file FAILED: %s", serializer.GetLastError().c_str());
                return false;
            }

            std::ifstream file(filepath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                LOG_ERROR("[Stress] Failed to open temp serialization file for reading.");
                return false;
            }
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileBuffer(size);
            if (!file.read(reinterpret_cast<char*>(fileBuffer.data()), size)) {
                LOG_ERROR("[Stress] Failed to read temp serialization file.");
                return false;
            }
            file.close();
            std::remove(filepath.c_str());

            ECSSnapshot* fileSnapshot = deserializer.DeserializeToSnapshot(fileBuffer.data(), fileBuffer.size(), registry);
            if (!fileSnapshot) {
                LOG_ERROR("[Stress] Deserialization from file buffer FAILED: %s", deserializer.GetLastError().c_str());
                return false;
            }

            if (!verifyEquivalence(origSnapshot, *fileSnapshot)) {
                LOG_ERROR("[Stress] Serialization file roundtrip verification FAILED!");
                delete fileSnapshot;
                return false;
            }
            delete fileSnapshot;

            ecs.Shutdown();
        }
        LOG_INFO("[Stress] Serialization Staging Integration Test passed successfully.");

        // -----------------------------------------------------------------------------
        // 4. GAMEPLAY AND PLAYER STATE INTEGRATION TEST
        // -----------------------------------------------------------------------------
        LOG_INFO("[Stress] Starting Gameplay and Player State Integration Test...");
        {
            auto world = std::make_unique<eng::runtime::World>();
            world->Initialize();

            // Create player entity
            ::Entity player = world->CreateEntity();

            eng::runtime::PlayerTagComponent ptc;
            ptc.active = true;
            world->AddComponent<eng::runtime::PlayerTagComponent>(player, ptc);

            eng::runtime::PlayerStateComponent psc;
            psc.Health = 100.0f;
            psc.IsAlive = true;
            world->AddComponent<eng::runtime::PlayerStateComponent>(player, psc);

            // Initialize Event Bus
            auto eventBus = std::make_unique<eng::runtime::GameplayEventBus>(nullptr);

            // Set up a runtime context
            eng::runtime::RuntimeContext context;
            context.ecs = world.get();
            context.scenes = nullptr;
            context.gameplayEventBus = eventBus.get();

            // Create game mode
            auto gameMode = std::make_unique<eng::runtime::VerticalSliceGameMode>();
            gameMode->OnLevelStart(&context);

            // Verify player discovery
            ::Entity discovered = gameMode->FindPlayerEntity();
            if (discovered != player) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Player discovery failed! Expected %u, got %u", player, discovered);
                world->Shutdown();
                return false;
            }

            // Verify initial state
            const auto& gs = gameMode->GetGameState();
            if (gs.SessionState != eng::runtime::GameSessionState::Playing) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected Playing state, got %d", (int)gs.SessionState);
                world->Shutdown();
                return false;
            }
            if (gs.ActiveObjectiveID != "OBJ_001") {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected objective OBJ_001, got %s", gs.ActiveObjectiveID.c_str());
                world->Shutdown();
                return false;
            }

            // Collect events to verify pub/sub
            std::vector<eng::runtime::GameplayEventType> receivedEvents;
            eventBus->Subscribe(eng::runtime::GameplayEventType::ObjectiveCompleted, [&](const eng::runtime::GameplayEvent& ev) {
                receivedEvents.push_back(ev.Type);
            });
            eventBus->Subscribe(eng::runtime::GameplayEventType::CheckpointReached, [&](const eng::runtime::GameplayEvent& ev) {
                receivedEvents.push_back(ev.Type);
            });
            eventBus->Subscribe(eng::runtime::GameplayEventType::LevelCompleted, [&](const eng::runtime::GameplayEvent& ev) {
                receivedEvents.push_back(ev.Type);
            });
            eventBus->Subscribe(eng::runtime::GameplayEventType::PlayerDied, [&](const eng::runtime::GameplayEvent& ev) {
                receivedEvents.push_back(ev.Type);
            });

            // 1. Progress OBJ_001 using TriggerEnter
            {
                eng::runtime::GameplayEvent ev;
                ev.Type = eng::runtime::GameplayEventType::TriggerEnter;
                ev.Source = player;
                ev.ObjectiveID = "OBJ_001_Trigger";
                eventBus->QueueEvent(ev);
            }
            
            // Queueing shouldn't process until flush
            if (gs.ActiveObjectiveID != "OBJ_001") {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Active objective changed before flush!");
                world->Shutdown();
                return false;
            }

            eventBus->FlushEvents();

            if (gs.ActiveObjectiveID != "OBJ_002") {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected OBJ_002, got %s", gs.ActiveObjectiveID.c_str());
                world->Shutdown();
                return false;
            }
            if (receivedEvents.size() != 1 || receivedEvents[0] != eng::runtime::GameplayEventType::ObjectiveCompleted) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected 1 ObjectiveCompleted event, got %zu", receivedEvents.size());
                world->Shutdown();
                return false;
            }

            // 2. Progress OBJ_002 using Interaction
            {
                eng::runtime::GameplayEvent ev;
                ev.Type = eng::runtime::GameplayEventType::Interaction;
                ev.Source = player;
                ev.Target = 42; // dummy target console
                eventBus->QueueEvent(ev);
            }
            eventBus->FlushEvents();

            if (gs.ActiveObjectiveID != "OBJ_003") {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected OBJ_003, got %s", gs.ActiveObjectiveID.c_str());
                world->Shutdown();
                return false;
            }

            // 3. Progress OBJ_003 using CheckpointReached
            {
                // First: Checkpoint trigger enter -> CheckpointReached event
                eng::runtime::GameplayEvent ev;
                ev.Type = eng::runtime::GameplayEventType::TriggerEnter;
                ev.Source = player;
                ev.ObjectiveID = "Checkpoint_CP_001";
                eventBus->QueueEvent(ev);
            }
            eventBus->FlushEvents(); // This should trigger CheckpointReached event
            
            // Queue has now a CheckpointReached event. Let's flush it.
            eventBus->FlushEvents();

            if (!gs.ActiveObjectiveID.empty()) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected empty active objective, got %s", gs.ActiveObjectiveID.c_str());
                world->Shutdown();
                return false;
            }
            if (gs.SessionState != eng::runtime::GameSessionState::Completed) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected Completed state, got %d", (int)gs.SessionState);
                world->Shutdown();
                return false;
            }
            if (gs.CurrentCheckpointID != "CP_001") {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected checkpoint CP_001, got %s", gs.CurrentCheckpointID.c_str());
                world->Shutdown();
                return false;
            }

            // 4. Test PlayerDied event flow
            // Restart game mode and verify full reset
            gameMode->RestartLevel();
            gameMode->OnLevelStart(&context);
            receivedEvents.clear();

            // Damage player to 0 HP
            {
                auto& coord = world->getCoordinator();
                auto& pState = coord.GetComponent<eng::runtime::PlayerStateComponent>(player);
                pState.Health = 0.0f;
            }

            // GameMode::Tick should detect health <= 0, and queue PlayerDied event
            gameMode->Tick(0.1f);
            
            if (gameMode->GetState() != eng::runtime::GameSessionState::Playing) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: State changed to Failed before event bus flush!");
                world->Shutdown();
                return false;
            }

            eventBus->FlushEvents();

            if (gameMode->GetState() != eng::runtime::GameSessionState::Failed) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected state to transition to Failed after flush, got %d", (int)gameMode->GetState());
                world->Shutdown();
                return false;
            }
            if (receivedEvents.size() != 1 || receivedEvents[0] != eng::runtime::GameplayEventType::PlayerDied) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Expected PlayerDied event, got %zu", receivedEvents.size());
                world->Shutdown();
                return false;
            }

            // 30x Play/Stop cycles
            LOG_INFO("[Stress] Running 30x Play/Stop stability cycles...");
            for (int i = 0; i < 30; ++i) {
                auto tempGameMode = std::make_unique<eng::runtime::VerticalSliceGameMode>();
                tempGameMode->OnLevelStart(&context);
                tempGameMode->Tick(0.016f);
                tempGameMode->OnLevelEnd();
            }

            // 30x Restart cycles
            LOG_INFO("[Stress] Running 30x Restart stability cycles...");
            for (int i = 0; i < 30; ++i) {
                gameMode->RestartLevel();
                gameMode->OnLevelStart(&context);
                gameMode->Tick(0.016f);
            }

            // Safe fallback with deleted player
            LOG_INFO("[Stress] Testing safe fallback with deleted player entity...");
            world->DestroyEntity(player);

            // Ticking the game mode after the player is deleted should NOT crash
            float timeBeforeTick = gameMode->GetGameState().ElapsedGameplayTime;
            gameMode->Tick(0.1f);

            // Check that ElapsedGameplayTime has not advanced (since tick should return early due to missing player)
            if (gameMode->GetGameState().ElapsedGameplayTime != timeBeforeTick) {
                LOG_ERROR("[Stress] Gameplay Test FAILED: Elapsed gameplay time advanced when player entity was deleted!");
                world->Shutdown();
                return false;
            }

            gameMode->OnLevelEnd();
            world->Shutdown();
        }
        LOG_INFO("[Stress] Gameplay and Player State Integration Test passed successfully.");

        // -----------------------------------------------------------------------------
        // 5. INTERACTION SYSTEM INTEGRATION TEST
        // -----------------------------------------------------------------------------
        {
            using namespace eng::runtime;
            LOG_INFO("[Stress] Starting Interaction System Integration Test...");

            auto world = std::make_unique<World>();
            world->Initialize();

            RuntimeContext context;
            context.mode = RuntimeMode::Editor;
            context.editorSimulationState = EditorSimulationState::Edit;
            context.ecs = world.get();

            GameplayEventBus eventBus;
            context.gameplayEventBus = &eventBus;

            InputManager inputManager;
            inputManager.Initialize();
            context.input = &inputManager;

            auto& coord = world->getCoordinator();
            auto interactionSys = world->GetSystem<InteractionSystem>();
            if (!interactionSys) {
                LOG_ERROR("[Stress] Interaction Test FAILED: InteractionSystem not found!");
                world->Shutdown();
                return false;
            }

            // Create player
            Entity player = world->CreateEntity();
            TransformComponent playerTrans;
            playerTrans.position = { 0.0f, 0.0f, 0.0f };
            world->AddComponent<TransformComponent>(player, playerTrans);

            CharacterControllerComponent playerCCC;
            playerCCC.yaw = -90.0f; // looking forward in negative Z
            playerCCC.pitch = 0.0f;
            world->AddComponent<CharacterControllerComponent>(player, playerCCC);

            PlayerStateComponent playerState;
            world->AddComponent<PlayerStateComponent>(player, playerState);

            // Create interactable
            Entity terminal = world->CreateEntity();
            TransformComponent termTrans;
            termTrans.position = { 0.0f, 0.0f, -1.5f }; // in front of player
            world->AddComponent<TransformComponent>(terminal, termTrans);

            InteractableComponent termInteract;
            termInteract.PromptText = "Use Terminal";
            termInteract.Enabled = true;
            termInteract.InteractionRadius = 2.0f;
            termInteract.Type = InteractionType::Use;
            world->AddComponent<InteractableComponent>(terminal, termInteract);

            // 1. Edit Mode restriction test
            interactionSys->Update(0.016f, context);
            auto& pState1 = coord.GetComponent<PlayerStateComponent>(player);
            if (pState1.CurrentInteractionTarget != INVALID_ENTITY || context.interactionPrompt.Visible) {
                LOG_ERROR("[Stress] Interaction Test FAILED: Target updated or prompt visible in Edit Mode!");
                world->Shutdown();
                return false;
            }

            // Switch to Play Mode
            context.editorSimulationState = EditorSimulationState::Play;

            // 2. Target selection test
            interactionSys->Update(0.016f, context);
            auto& pState2 = coord.GetComponent<PlayerStateComponent>(player);
            if (pState2.CurrentInteractionTarget != terminal) {
                LOG_ERROR("[Stress] Interaction Test FAILED: Target was not resolved to terminal! Got %u, expected %u",
                          pState2.CurrentInteractionTarget, terminal);
                world->Shutdown();
                return false;
            }
            if (!context.interactionPrompt.Visible || context.interactionPrompt.Text != "Use Terminal" || context.interactionPrompt.Target != terminal) {
                LOG_ERROR("[Stress] Interaction Test FAILED: InteractionPromptData not populated correctly!");
                world->Shutdown();
                return false;
            }

            // 3. Disabled interactable test
            coord.GetComponent<InteractableComponent>(terminal).Enabled = false;
            interactionSys->Update(0.016f, context);
            auto& pState3 = coord.GetComponent<PlayerStateComponent>(player);
            if (pState3.CurrentInteractionTarget != INVALID_ENTITY || context.interactionPrompt.Visible) {
                LOG_ERROR("[Stress] Interaction Test FAILED: Target resolved or prompt shown for disabled interactable!");
                world->Shutdown();
                return false;
            }
            coord.GetComponent<InteractableComponent>(terminal).Enabled = true; // reset

            // 4. Destroyed interactable test
            interactionSys->Update(0.016f, context); // set target again
            world->DestroyEntity(terminal);
            interactionSys->Update(0.016f, context); // process destroyed
            auto& pState4 = coord.GetComponent<PlayerStateComponent>(player);
            if (pState4.CurrentInteractionTarget != INVALID_ENTITY || context.interactionPrompt.Visible) {
                LOG_ERROR("[Stress] Interaction Test FAILED: Target not cleared after interactable was destroyed!");
                world->Shutdown();
                return false;
            }

            // Recreate terminal for event validation
            terminal = world->CreateEntity();
            termTrans.position = { 0.0f, 0.0f, -1.5f };
            world->AddComponent<TransformComponent>(terminal, termTrans);
            world->AddComponent<InteractableComponent>(terminal, termInteract);

            // 5. Event validation test
            std::vector<GameplayEventType> receivedEvents;
            eventBus.Subscribe(GameplayEventType::Interaction, [&](const GameplayEvent& event) {
                if (event.Source == player && event.Target == terminal) {
                    receivedEvents.push_back(event.Type);
                }
            });

            // Simulate press E
            inputManager.SetActionStateForTest("Interact", true);
            interactionSys->Update(0.016f, context);
            eventBus.FlushEvents();

            if (receivedEvents.size() != 1) {
                LOG_ERROR("[Stress] Interaction Test FAILED: Expected exactly 1 interaction event, got %zu!", receivedEvents.size());
                world->Shutdown();
                return false;
            }

            world->Shutdown();
            LOG_INFO("[Stress] Interaction System Integration Test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // 6. OBJECTIVE SYSTEM INTEGRATION TEST
        // -----------------------------------------------------------------------------
        {
            using namespace eng::runtime;
            LOG_INFO("[Stress] Starting Objective System Integration Test...");

            auto world = std::make_unique<World>();
            world->Initialize();

            RuntimeContext context;
            context.mode = RuntimeMode::Editor;
            context.editorSimulationState = EditorSimulationState::Edit;
            context.ecs = world.get();

            GameplayEventBus eventBus;
            context.gameplayEventBus = &eventBus;

            auto gameMode = std::make_unique<VerticalSliceGameMode>();
            context.gameMode = gameMode.get();

            auto objectiveSys = std::make_unique<ObjectiveSystem>();
            objectiveSys->Initialize(&context);

            auto& coord = world->getCoordinator();

            // Create entities with ObjectiveComponents
            Entity terminal = world->CreateEntity();
            ObjectiveComponent termObj;
            termObj.ObjectiveID = "OBJ_USE_TERMINAL";
            termObj.Title = "Use Terminal";
            termObj.Description = "Interact with the terminal to unlock the door";
            termObj.CompletionMode = ObjectiveCompletionMode::Interaction;
            termObj.StartsActive = true;
            termObj.Repeatable = false;
            termObj.Completed = false;
            world->AddComponent<ObjectiveComponent>(terminal, termObj);

            Entity escapeZone = world->CreateEntity();
            ObjectiveComponent escapeObj;
            escapeObj.ObjectiveID = "OBJ_REACH_EXIT";
            escapeObj.Title = "Reach Exit";
            escapeObj.Description = "Find the way out";
            escapeObj.CompletionMode = ObjectiveCompletionMode::TriggerEnter;
            escapeObj.StartsActive = false; // Starts inactive
            escapeObj.Repeatable = false;
            escapeObj.Completed = false;
            world->AddComponent<ObjectiveComponent>(escapeZone, escapeObj);

            // 1. Edit Mode Restriction Test
            objectiveSys->OnLevelStart();
            if (!objectiveSys->GetObjectives().empty()) {
                LOG_ERROR("[Stress] Objective Test FAILED: Objectives registered in Edit Mode!");
                world->Shutdown();
                return false;
            }

            // Switch to Play Mode
            context.mode = RuntimeMode::Game;

            // 2. Play Mode start activation test
            objectiveSys->OnLevelStart();
            const auto& registry = objectiveSys->GetObjectives();
            if (registry.size() != 2) {
                LOG_ERROR("[Stress] Objective Test FAILED: Expected 2 registered objectives, got %zu!", registry.size());
                world->Shutdown();
                return false;
            }

            auto itTerm = registry.find("OBJ_USE_TERMINAL");
            auto itEscape = registry.find("OBJ_REACH_EXIT");
            if (itTerm == registry.end() || itEscape == registry.end()) {
                LOG_ERROR("[Stress] Objective Test FAILED: Objectives not found in registry!");
                world->Shutdown();
                return false;
            }

            if (itTerm->second.State != ObjectiveState::Active || itEscape->second.State != ObjectiveState::Inactive) {
                LOG_ERROR("[Stress] Objective Test FAILED: Initial objective states incorrect!");
                world->Shutdown();
                return false;
            }

            auto& gs = gameMode->GetGameState();
            if (gs.ActiveObjectiveID != "OBJ_USE_TERMINAL") {
                LOG_ERROR("[Stress] Objective Test FAILED: GameState active objective was not updated on start!");
                world->Shutdown();
                return false;
            }

            // 3. Interaction completion test
            {
                GameplayEvent ev;
                ev.Type = GameplayEventType::Interaction;
                ev.Source = 1; // dummy player
                ev.Target = terminal;
                eventBus.QueueEvent(ev);
            }
            eventBus.FlushEvents();

            if (itTerm->second.State != ObjectiveState::Completed || !gs.ActiveObjectiveID.empty()) {
                LOG_ERROR("[Stress] Objective Test FAILED: Interaction objective did not complete correctly!");
                world->Shutdown();
                return false;
            }

            bool foundCompleted = false;
            for (const auto& completed : gs.CompletedObjectives) {
                if (completed == "OBJ_USE_TERMINAL") foundCompleted = true;
            }
            if (!foundCompleted) {
                LOG_ERROR("[Stress] Objective Test FAILED: OBJ_USE_TERMINAL not added to GameState completed list!");
                world->Shutdown();
                return false;
            }

            // 4. Inactive objective completion prevention
            {
                GameplayEvent ev;
                ev.Type = GameplayEventType::TriggerEnter;
                ev.Source = 1;
                ev.Target = escapeZone;
                ev.ObjectiveID = "OBJ_REACH_EXIT"; // trigger event name matches
                eventBus.QueueEvent(ev);
            }
            eventBus.FlushEvents();

            if (itEscape->second.State != ObjectiveState::Inactive) {
                LOG_ERROR("[Stress] Objective Test FAILED: Inactive objective was completed by event!");
                world->Shutdown();
                return false;
            }

            // 5. Trigger-based completion (after activation)
            objectiveSys->StartObjective("OBJ_REACH_EXIT");
            if (itEscape->second.State != ObjectiveState::Active || gs.ActiveObjectiveID != "OBJ_REACH_EXIT") {
                LOG_ERROR("[Stress] Objective Test FAILED: Failed to activate escape objective!");
                world->Shutdown();
                return false;
            }

            {
                GameplayEvent ev;
                ev.Type = GameplayEventType::TriggerEnter;
                ev.Source = 1;
                ev.Target = escapeZone;
                ev.ObjectiveID = "OBJ_REACH_EXIT";
                eventBus.QueueEvent(ev);
            }
            eventBus.FlushEvents();

            if (itEscape->second.State != ObjectiveState::Completed) {
                LOG_ERROR("[Stress] Objective Test FAILED: Escape objective was not completed by trigger overlap!");
                world->Shutdown();
                return false;
            }

            // 6. Duplicate Completion Prevention
            size_t initialCompletedSize = gs.CompletedObjectives.size();
            objectiveSys->CompleteObjective("OBJ_USE_TERMINAL"); // try complete again
            if (gs.CompletedObjectives.size() != initialCompletedSize) {
                LOG_ERROR("[Stress] Objective Test FAILED: Non-repeatable objective completed twice!");
                world->Shutdown();
                return false;
            }

            // 7. Restart Reset Test
            gameMode->RestartLevel(); // resets GameState
            objectiveSys->OnLevelRestart();

            if (!gs.CompletedObjectives.empty()) {
                LOG_ERROR("[Stress] Objective Test FAILED: CompletedObjectives list not cleared on restart!");
                world->Shutdown();
                return false;
            }

            const auto& postRegistry = objectiveSys->GetObjectives();
            auto itTermPost = postRegistry.find("OBJ_USE_TERMINAL");
            auto itEscapePost = postRegistry.find("OBJ_REACH_EXIT");
            if (itTermPost == postRegistry.end() || itEscapePost == postRegistry.end() ||
                itTermPost->second.State != ObjectiveState::Active || itEscapePost->second.State != ObjectiveState::Inactive) {
                LOG_ERROR("[Stress] Objective Test FAILED: Objective states not reset on restart!");
                world->Shutdown();
                return false;
            }


            world->Shutdown();
            LOG_INFO("[Stress] Objective System Integration Test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // 7. HUD SYSTEM INTEGRATION TEST
        // -----------------------------------------------------------------------------
        {
            using namespace eng::runtime;
            LOG_INFO("[Stress] Starting HUD System Integration Test...");

            // Ensure ImGui context is active for HUD drawing tests
            ImGuiContext* previousContext = ImGui::GetCurrentContext();
            ImGuiContext* testContext = nullptr;
            if (previousContext == nullptr) {
                testContext = ImGui::CreateContext();
                ImGui::SetCurrentContext(testContext);
            }

            // Build font atlas to prevent assertions when drawing in mocked contexts
            if (ImGui::GetCurrentContext() != nullptr) {
                unsigned char* pixels;
                int width, height;
                ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            }

            auto world = std::make_unique<World>();
            world->Initialize();

            RuntimeContext context;
            context.mode = RuntimeMode::Editor;
            context.editorSimulationState = EditorSimulationState::Edit;
            context.ecs = world.get();

            GameplayEventBus eventBus;
            context.gameplayEventBus = &eventBus;

            auto gameMode = std::make_unique<VerticalSliceGameMode>();
            context.gameMode = gameMode.get();

            // Setup ImGui for rendering
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(1280, 720);
            ImGui::NewFrame();

            // 1. Creation & Edit Mode Isolation
            gameMode->OnLevelStart(&context);
            GameplayHUD* hud = gameMode->GetGameplayHUD();
            if (!hud) {
                LOG_ERROR("[Stress] HUD Test FAILED: GameplayHUD was not created by GameMode!");
                world->Shutdown();
                if (testContext) ImGui::DestroyContext(testContext);
                return false;
            }

            // Let's verify visibility defaults
            if (!hud->IsVisible()) {
                LOG_ERROR("[Stress] HUD Test FAILED: HUD should default to visible!");
                world->Shutdown();
                if (testContext) ImGui::DestroyContext(testContext);
                return false;
            }

            // 2. Play mode state connection & rendering
            context.mode = RuntimeMode::Game;
            context.editorSimulationState = EditorSimulationState::Play;

            auto& gs = gameMode->GetGameStateMutable();
            gs.ActiveObjectiveID = "OBJ_USE_TERMINAL";

            // Set up player state
            auto& coord = world->getCoordinator();
            Entity player = world->CreateEntity();
            
            PlayerTagComponent tag;
            coord.AddComponent<PlayerTagComponent>(player, tag);
            
            PlayerStateComponent psc;
            psc.Health = 80.0f;
            psc.MaxHealth = 100.0f;
            psc.IsAlive = true;
            coord.AddComponent<PlayerStateComponent>(player, psc);

            // Trigger HUD update to bind context and update timers
            gameMode->Tick(0.016f);

            // Verify F9 toggle logic using AddKeyEvent
            ImGui::EndFrame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F9, true);
            ImGui::NewFrame();
            hud->Update(0.016f);
            if (hud->IsVisible()) {
                LOG_ERROR("[Stress] HUD Test FAILED: F9 key did not toggle visibility off!");
                world->Shutdown();
                if (testContext) ImGui::DestroyContext(testContext);
                return false;
            }

            // Release F9 key
            ImGui::EndFrame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F9, false);
            ImGui::NewFrame();
            hud->Update(0.016f);

            // Toggle back on
            ImGui::EndFrame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F9, true);
            ImGui::NewFrame();
            hud->Update(0.016f);
            if (!hud->IsVisible()) {
                LOG_ERROR("[Stress] HUD Test FAILED: F9 key did not toggle visibility on!");
                world->Shutdown();
                if (testContext) ImGui::DestroyContext(testContext);
                return false;
            }

            // Release the key and clear frame state
            ImGui::EndFrame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_F9, false);
            ImGui::NewFrame();

            // 3. Notification triggers & timers
            hud->ShowNotification("Test Notification", 1.0f);
            hud->Update(0.4f);
            hud->Update(0.7f);
            
            // 4. Checkpoint & Objective complete event notifications
            GameplayEvent cpEvent;
            cpEvent.Type = GameplayEventType::CheckpointReached;
            cpEvent.CheckpointID = "CP_001";
            eventBus.Publish(cpEvent); // directly publish to trigger HUD subscriber
            
            // 5. Test health DEAD state
            auto& mutablePsc = coord.GetComponent<PlayerStateComponent>(player);
            mutablePsc.Health = 0.0f;
            mutablePsc.IsAlive = false;

            // Run rendering code to make sure it doesn't crash on null elements or dead state
            hud->Render(0.0f, 0.0f, 1280.0f, 720.0f);

            ImGui::EndFrame();

            // Cleanup level
            gameMode->OnLevelEnd();

            world->Shutdown();
            
            // Restore previous context if we created a temporary one
            if (testContext) {
                ImGui::DestroyContext(testContext);
                ImGui::SetCurrentContext(previousContext);
            }

            LOG_INFO("[Stress] HUD System Integration Test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // 8. AUDIO SYSTEM INTEGRATION TEST
        // -----------------------------------------------------------------------------
        {
            using namespace eng::runtime;
            LOG_INFO("[Stress] Starting Audio System Integration Test...");

            // Test 1: Initialize/Shutdown & Double shutdown safety
            {
                AudioSystem audio;
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                bool initResult = audio.Initialize(&context);
                if (!initResult || !audio.IsInitialized()) {
                    LOG_ERROR("[Stress] Audio Test 1 FAILED: Initialization failed!");
                    return false;
                }
                audio.Shutdown();
                if (audio.IsInitialized()) {
                    LOG_ERROR("[Stress] Audio Test 1 FAILED: Shutdown did not reset initialization flag!");
                    return false;
                }
                audio.Shutdown(); // Double shutdown safety check
            }

            // Test 2: WAV Playback & Missing File Safety
            {
                AudioSystem audio;
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                audio.Initialize(&context);

                auto handle = audio.PlayOneShot("Assets/Audio/test_beep.wav", 1.0f);
                if (handle == 0) {
                    LOG_ERROR("[Stress] Audio Test 2 FAILED: Failed to play valid synthetic WAV!");
                    return false;
                }
                audio.StopSound(handle);

                // Play missing WAV file
                auto missingHandle = audio.PlayOneShot("Assets/Audio/nonexistent_beep.wav", 1.0f);
                if (missingHandle != 0) {
                    LOG_ERROR("[Stress] Audio Test 2 FAILED: Playing missing file did not fail cleanly!");
                    return false;
                }
                audio.Shutdown();
            }

            // Test 3: Event Subscriptions
            {
                AudioSystem audio;
                RuntimeContext context;
                context.mode = RuntimeMode::Game; // Simulating active play mode
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                audio.Initialize(&context);

                // Tick updated to handle play modes transitions
                audio.Update(0.1f);

                // Emit Interaction
                GameplayEvent intEvent;
                intEvent.Type = GameplayEventType::Interaction;
                eventBus.Publish(intEvent);
                eventBus.FlushEvents();
                audio.Update(0.1f);
                if (audio.GetLastPlayedClip() != "Assets/Audio/interaction_use.wav") {
                    LOG_ERROR("[Stress] Audio Test 3 FAILED: Event-driven interaction sound did not trigger! Got: %s", audio.GetLastPlayedClip().c_str());
                    return false;
                }

                // Emit TriggerEnter
                GameplayEvent trigEvent;
                trigEvent.Type = GameplayEventType::TriggerEnter;
                eventBus.Publish(trigEvent);
                eventBus.FlushEvents();
                audio.Update(0.1f);
                if (audio.GetLastPlayedClip() != "Assets/Audio/trigger_enter.wav") {
                    LOG_ERROR("[Stress] Audio Test 3 FAILED: Event-driven trigger sound did not trigger! Got: %s", audio.GetLastPlayedClip().c_str());
                    return false;
                }

                audio.Shutdown();
            }

            // Test 4: Play/Edit Mode Isolation
            {
                AudioSystem audio;
                RuntimeContext context;
                context.mode = RuntimeMode::Editor;
                context.editorSimulationState = EditorSimulationState::Edit;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                audio.Initialize(&context);

                audio.Update(0.1f);

                // Interaction event in Edit Mode
                GameplayEvent intEvent;
                intEvent.Type = GameplayEventType::Interaction;
                eventBus.Publish(intEvent);
                eventBus.FlushEvents();
                audio.Update(0.1f);
                if (audio.GetLastPlayedClip() != "None") {
                    LOG_ERROR("[Stress] Audio Test 4 FAILED: Sound played during Edit Mode! Got: %s", audio.GetLastPlayedClip().c_str());
                    return false;
                }

                // Transition to Play Mode
                context.editorSimulationState = EditorSimulationState::Play;
                audio.Update(0.1f);

                eventBus.Publish(intEvent);
                eventBus.FlushEvents();
                audio.Update(0.1f);
                if (audio.GetLastPlayedClip() != "Assets/Audio/interaction_use.wav") {
                    LOG_ERROR("[Stress] Audio Test 4 FAILED: Sound failed to play after transitioning to Play Mode!");
                    return false;
                }

                // Transition back to Edit Mode
                context.editorSimulationState = EditorSimulationState::Edit;
                audio.Update(0.1f); // Triggers Edit transition
                if (audio.GetActiveSoundsCount() != 0) {
                    LOG_ERROR("[Stress] Audio Test 4 FAILED: Active sound streams were not stopped on simulation stop!");
                    return false;
                }

                audio.Shutdown();
            }

            // Test 5: Volume Configuration clamping
            {
                AudioSystem audio;
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                audio.Initialize(&context);

                audio.SetMasterVolume(1.5f);
                if (audio.GetMasterVolume() != 1.0f) {
                    LOG_ERROR("[Stress] Audio Test 5 FAILED: Master volume failed to clamp to 1.0!");
                    return false;
                }

                audio.SetMasterVolume(-0.5f);
                if (audio.GetMasterVolume() != 0.0f) {
                    LOG_ERROR("[Stress] Audio Test 5 FAILED: Master volume failed to clamp to 0.0!");
                    return false;
                }

                audio.SetMasterVolume(0.7f);
                if (std::abs(audio.GetMasterVolume() - 0.7f) > 0.01f) {
                    LOG_ERROR("[Stress] Audio Test 5 FAILED: Master volume failed to set correct volume! Got: %.2f", audio.GetMasterVolume());
                    return false;
                }

                audio.Shutdown();
            }

            // Test 6: AudioSourceComponent Serialization/Deserialization Roundtrip
            {
                if (!AudioSystem::TestSerialization()) {
                    return false;
                }
            }

            LOG_INFO("[Stress] Audio System Integration Test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // 9. STATE OBJECTS & DOOR ACTIVATION SYSTEM INTEGRATION TEST
        // -----------------------------------------------------------------------------
        {
            using namespace eng::runtime;
            LOG_INFO("[Stress] Starting State Objects & Door Activation System Integration Test...");

            // Test 1: Terminal Opens Door (Instant)
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                // Setup ECS World
                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                auto& coordinator = world->getCoordinator();

                // Create terminal entity
                Entity terminal = world->CreateEntity();
                NameComponent termName; termName.name = "Terminal";
                world->AddComponent<NameComponent>(terminal, termName);
                
                InteractableComponent interactable;
                world->AddComponent<InteractableComponent>(terminal, interactable);

                ActivatableComponent termActivator;
                termActivator.TargetActivationID = "DOOR_A";
                termActivator.OneShot = true;
                termActivator.HasActivated = false;
                world->AddComponent<ActivatableComponent>(terminal, termActivator);

                // Create door entity
                Entity door = world->CreateEntity();
                NameComponent doorName; doorName.name = "DoorA";
                world->AddComponent<NameComponent>(door, doorName);

                TransformComponent transform;
                transform.position = Vector3(10.0f, 0.0f, 0.0f);
                world->AddComponent<TransformComponent>(door, transform);

                SimpleStateComponent doorState;
                doorState.InitialState = SimpleObjectState::Locked;
                doorState.CurrentState = SimpleObjectState::Locked;
                doorState.ResetOnPlay = true;
                world->AddComponent<SimpleStateComponent>(door, doorState);

                ActivatableComponent doorActivator;
                doorActivator.ActivationID = "DOOR_A";
                world->AddComponent<ActivatableComponent>(door, doorActivator);

                DoorComponent doorComp;
                doorComp.OpenMode = DoorOpenMode::Instant;
                doorComp.ClosedPosition = Vector3(10.0f, 0.0f, 0.0f);
                doorComp.OpenOffset = Vector3(0.0f, 5.0f, 0.0f);
                doorComp.IsOpen = false;
                doorComp.IsOpening = false;
                world->AddComponent<DoorComponent>(door, doorComp);

                ObjectActivationSystem system;
                system.Initialize(&context);
                system.OnPlayStart();

                // Check starting state
                const auto& stateBefore = coordinator.GetComponent<SimpleStateComponent>(door);
                if (stateBefore.CurrentState != SimpleObjectState::Locked) {
                    LOG_ERROR("[Stress] State Test 1 FAILED: Door not initialized to Locked!");
                    return false;
                }

                // Simulate interaction event
                GameplayEvent event;
                event.Type = GameplayEventType::Interaction;
                event.Target = terminal;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                // Tick
                system.Update(0.1f);

                // Verify door opened and transitioned to Completed
                const auto& stateAfter = coordinator.GetComponent<SimpleStateComponent>(door);
                const auto& doorAfter = coordinator.GetComponent<DoorComponent>(door);
                const auto& transformAfter = coordinator.GetComponent<TransformComponent>(door);
                const auto& termActivatorAfter = coordinator.GetComponent<ActivatableComponent>(terminal);

                if (stateAfter.CurrentState != SimpleObjectState::Completed) {
                    LOG_ERROR("[Stress] State Test 1 FAILED: Door state was not transitioned to Completed! Got: %s", SimpleObjectStateToString(stateAfter.CurrentState).c_str());
                    return false;
                }
                if (!doorAfter.IsOpen) {
                    LOG_ERROR("[Stress] State Test 1 FAILED: Door IsOpen was not set to true!");
                    return false;
                }
                if (transformAfter.position.y != 5.0f) {
                    LOG_ERROR("[Stress] State Test 1 FAILED: Door position was not instant opened to offset! y: %.2f", transformAfter.position.y);
                    return false;
                }
                if (!termActivatorAfter.HasActivated) {
                    LOG_ERROR("[Stress] State Test 1 FAILED: Terminal HasActivated flag was not set to true!");
                    return false;
                }

                system.OnPlayStop();
                world->Shutdown();
            }

            // Test 2: Smooth Door Movement
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                auto& coordinator = world->getCoordinator();

                Entity terminal = world->CreateEntity();
                ActivatableComponent termActivator;
                termActivator.TargetActivationID = "DOOR_B";
                termActivator.OneShot = true;
                world->AddComponent<ActivatableComponent>(terminal, termActivator);

                Entity door = world->CreateEntity();
                TransformComponent transform;
                transform.position = Vector3(0.0f, 0.0f, 0.0f);
                world->AddComponent<TransformComponent>(door, transform);

                SimpleStateComponent doorState;
                doorState.InitialState = SimpleObjectState::Inactive;
                doorState.CurrentState = SimpleObjectState::Inactive;
                world->AddComponent<SimpleStateComponent>(door, doorState);

                ActivatableComponent doorActivator;
                doorActivator.ActivationID = "DOOR_B";
                world->AddComponent<ActivatableComponent>(door, doorActivator);

                DoorComponent doorComp;
                doorComp.OpenMode = DoorOpenMode::Smooth;
                doorComp.ClosedPosition = Vector3(0.0f, 0.0f, 0.0f);
                doorComp.OpenOffset = Vector3(0.0f, 10.0f, 0.0f);
                doorComp.OpenSpeed = 2.0f; // lerps by 2 * dt
                doorComp.IsOpen = false;
                doorComp.IsOpening = false;
                world->AddComponent<DoorComponent>(door, doorComp);

                ObjectActivationSystem system;
                system.Initialize(&context);
                system.OnPlayStart();

                // Trigger activation
                GameplayEvent event;
                event.Type = GameplayEventType::Interaction;
                event.Target = terminal;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                // Tick once to start moving
                system.Update(0.1f);

                const auto& doorMoving = coordinator.GetComponent<DoorComponent>(door);
                const auto& stateMoving = coordinator.GetComponent<SimpleStateComponent>(door);
                const auto& transMoving = coordinator.GetComponent<TransformComponent>(door);

                if (!doorMoving.IsOpening) {
                    LOG_ERROR("[Stress] State Test 2 FAILED: Smooth door did not set IsOpening!");
                    return false;
                }
                if (stateMoving.CurrentState != SimpleObjectState::Active) {
                    LOG_ERROR("[Stress] State Test 2 FAILED: Smooth door did not transition to Active state!");
                    return false;
                }
                if (transMoving.position.y <= 0.0f || transMoving.position.y >= 10.0f) {
                    LOG_ERROR("[Stress] State Test 2 FAILED: Smooth door position did not update or immediately teleported! y: %.2f", transMoving.position.y);
                    return false;
                }

                // Simulate ticking until fully open (lerping towards 10.0f)
                for (int i = 0; i < 50; ++i) {
                    system.Update(0.1f);
                }

                const auto& doorFinished = coordinator.GetComponent<DoorComponent>(door);
                const auto& stateFinished = coordinator.GetComponent<SimpleStateComponent>(door);
                const auto& transFinished = coordinator.GetComponent<TransformComponent>(door);

                if (!doorFinished.IsOpen) {
                    LOG_ERROR("[Stress] State Test 2 FAILED: Smooth door failed to set IsOpen after sufficient ticks!");
                    return false;
                }
                if (doorFinished.IsOpening) {
                    LOG_ERROR("[Stress] State Test 2 FAILED: Smooth door IsOpening flag is still true!");
                    return false;
                }
                if (stateFinished.CurrentState != SimpleObjectState::Completed) {
                    LOG_ERROR("[Stress] State Test 2 FAILED: Smooth door state was not Completed! Got: %s", SimpleObjectStateToString(stateFinished.CurrentState).c_str());
                    return false;
                }
                if (std::abs(transFinished.position.y - 10.0f) > 0.01f) {
                    LOG_ERROR("[Stress] State Test 2 FAILED: Smooth door failed to clamp to exact target! y: %.2f", transFinished.position.y);
                    return false;
                }

                system.OnPlayStop();
                world->Shutdown();
            }

            // Test 3: Locked Constraints (does not open on its own or direct trigger)
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                auto& coordinator = world->getCoordinator();

                Entity door = world->CreateEntity();
                TransformComponent transform;
                transform.position = Vector3(0.0f, 0.0f, 0.0f);
                world->AddComponent<TransformComponent>(door, transform);

                SimpleStateComponent doorState;
                doorState.InitialState = SimpleObjectState::Locked;
                doorState.CurrentState = SimpleObjectState::Locked;
                world->AddComponent<SimpleStateComponent>(door, doorState);

                DoorComponent doorComp;
                doorComp.OpenMode = DoorOpenMode::Instant;
                doorComp.ClosedPosition = Vector3(0.0f, 0.0f, 0.0f);
                doorComp.OpenOffset = Vector3(0.0f, 5.0f, 0.0f);
                doorComp.IsOpen = false;
                world->AddComponent<DoorComponent>(door, doorComp);

                ObjectActivationSystem system;
                system.Initialize(&context);
                system.OnPlayStart();

                // Update without event
                system.Update(0.1f);

                const auto& doorAfter = coordinator.GetComponent<DoorComponent>(door);
                const auto& stateAfter = coordinator.GetComponent<SimpleStateComponent>(door);
                if (doorAfter.IsOpen || stateAfter.CurrentState != SimpleObjectState::Locked) {
                    LOG_ERROR("[Stress] State Test 3 FAILED: Locked door opened without any activation!");
                    return false;
                }

                system.OnPlayStop();
                world->Shutdown();
            }

            // Test 4: Play Restart Reset & Caching Closed Position
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                auto& coordinator = world->getCoordinator();

                Entity terminal = world->CreateEntity();
                ActivatableComponent termActivator;
                termActivator.TargetActivationID = "DOOR_D";
                termActivator.OneShot = true;
                world->AddComponent<ActivatableComponent>(terminal, termActivator);

                Entity door = world->CreateEntity();
                TransformComponent transform;
                transform.position = Vector3(5.0f, 5.0f, 5.0f); // Closed Position is cached on play start!
                world->AddComponent<TransformComponent>(door, transform);

                SimpleStateComponent doorState;
                doorState.InitialState = SimpleObjectState::Locked;
                doorState.CurrentState = SimpleObjectState::Locked;
                doorState.ResetOnPlay = true;
                world->AddComponent<SimpleStateComponent>(door, doorState);

                ActivatableComponent doorActivator;
                doorActivator.ActivationID = "DOOR_D";
                world->AddComponent<ActivatableComponent>(door, doorActivator);

                DoorComponent doorComp;
                doorComp.OpenMode = DoorOpenMode::Instant;
                doorComp.OpenOffset = Vector3(0.0f, 10.0f, 0.0f);
                world->AddComponent<DoorComponent>(door, doorComp);

                ObjectActivationSystem system;
                system.Initialize(&context);

                // Play cycle 1
                system.OnPlayStart();

                // Trigger interaction
                GameplayEvent event;
                event.Type = GameplayEventType::Interaction;
                event.Target = terminal;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                system.Update(0.1f);

                if (coordinator.GetComponent<TransformComponent>(door).position.y != 15.0f) {
                    LOG_ERROR("[Stress] State Test 4 FAILED: Door did not open in Cycle 1!");
                    return false;
                }

                // Stop Play mode (must restore position!)
                system.OnPlayStop();

                if (coordinator.GetComponent<TransformComponent>(door).position.y != 5.0f) {
                    LOG_ERROR("[Stress] State Test 4 FAILED: OnPlayStop failed to restore door position to closed!");
                    return false;
                }

                // Restart Play cycle 2
                system.OnPlayStart();

                // Verify states were reset
                const auto& stateReset = coordinator.GetComponent<SimpleStateComponent>(door);
                const auto& doorReset = coordinator.GetComponent<DoorComponent>(door);
                const auto& termReset = coordinator.GetComponent<ActivatableComponent>(terminal);

                if (stateReset.CurrentState != SimpleObjectState::Locked) {
                    LOG_ERROR("[Stress] State Test 4 FAILED: CurrentState did not reset to Locked on Play restart!");
                    return false;
                }
                if (doorReset.IsOpen) {
                    LOG_ERROR("[Stress] State Test 4 FAILED: Door IsOpen did not reset to false!");
                    return false;
                }
                if (termReset.HasActivated) {
                    LOG_ERROR("[Stress] State Test 4 FAILED: Terminal HasActivated did not reset to false!");
                    return false;
                }

                system.OnPlayStop();
                world->Shutdown();
            }

            // Test 5: Edit Mode Isolation
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                
                // Set to Edit Mode
                context.mode = RuntimeMode::Editor;
                context.editorSimulationState = EditorSimulationState::Edit;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                auto& coordinator = world->getCoordinator();

                Entity terminal = world->CreateEntity();
                ActivatableComponent termActivator;
                termActivator.TargetActivationID = "DOOR_E";
                world->AddComponent<ActivatableComponent>(terminal, termActivator);

                Entity door = world->CreateEntity();
                TransformComponent transform;
                transform.position = Vector3(0.0f, 0.0f, 0.0f);
                world->AddComponent<TransformComponent>(door, transform);

                SimpleStateComponent doorState;
                doorState.InitialState = SimpleObjectState::Inactive;
                doorState.CurrentState = SimpleObjectState::Inactive;
                world->AddComponent<SimpleStateComponent>(door, doorState);

                ActivatableComponent doorActivator;
                doorActivator.ActivationID = "DOOR_E";
                world->AddComponent<ActivatableComponent>(door, doorActivator);

                DoorComponent doorComp;
                doorComp.OpenMode = DoorOpenMode::Instant;
                doorComp.OpenOffset = Vector3(0.0f, 5.0f, 0.0f);
                world->AddComponent<DoorComponent>(door, doorComp);

                ObjectActivationSystem system;
                system.Initialize(&context);
                system.OnPlayStart();

                // Trigger interaction in Edit mode
                GameplayEvent event;
                event.Type = GameplayEventType::Interaction;
                event.Target = terminal;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                system.Update(0.1f);

                // Position and state should NOT change since it is Edit Mode
                if (coordinator.GetComponent<TransformComponent>(door).position.y != 0.0f) {
                    LOG_ERROR("[Stress] State Test 5 FAILED: Door position changed during Edit Mode!");
                    return false;
                }
                if (coordinator.GetComponent<SimpleStateComponent>(door).CurrentState != SimpleObjectState::Inactive) {
                    LOG_ERROR("[Stress] State Test 5 FAILED: Door state updated during Edit Mode!");
                    return false;
                }

                system.OnPlayStop();
                world->Shutdown();
            }

            // Test 6: Missing Target Safety
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                Entity terminal = world->CreateEntity();
                ActivatableComponent termActivator;
                termActivator.TargetActivationID = "DOOR_NONEXISTENT";
                world->AddComponent<ActivatableComponent>(terminal, termActivator);

                ObjectActivationSystem system;
                system.Initialize(&context);
                system.OnPlayStart();

                GameplayEvent event;
                event.Type = GameplayEventType::Interaction;
                event.Target = terminal;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                system.Update(0.1f);

                // Verify last error diagnostics
                if (system.GetLastActivationError() != "Target not found: DOOR_NONEXISTENT") {
                    LOG_ERROR("[Stress] State Test 6 FAILED: Failed to track activation error diagnostic! Got: %s", system.GetLastActivationError().c_str());
                    return false;
                }

                system.OnPlayStop();
                world->Shutdown();
            }

            LOG_INFO("[Stress] State Objects & Door Activation System Integration Test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // 10. CHECKPOINT SYSTEM INTEGRATION TEST
        // -----------------------------------------------------------------------------
        {
            using namespace eng::runtime;
            LOG_INFO("[Stress] Starting Checkpoint System Integration Test...");

            // Test 1: Basic Checkpoint Activation
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                auto& coordinator = world->getCoordinator();

                // Setup player
                Entity player = world->CreateEntity();
                PlayerStateComponent psc;
                psc.Health = 100.0f;
                psc.IsAlive = true;
                world->AddComponent<PlayerStateComponent>(player, psc);
                
                PlayerTagComponent tag;
                world->AddComponent<PlayerTagComponent>(player, tag);

                TransformComponent pTrans;
                pTrans.position = Vector3(1.0f, 2.0f, 3.0f);
                world->AddComponent<TransformComponent>(player, pTrans);

                // Setup checkpoint
                Entity checkpoint = world->CreateEntity();
                CheckpointComponent cc;
                cc.CheckpointID = "CP_BUNKER";
                cc.CheckpointName = "Bunker Entrance";
                cc.ActivateOnTriggerEnter = true;
                cc.OneShot = false;
                cc.HasActivated = false;
                world->AddComponent<CheckpointComponent>(checkpoint, cc);

                VerticalSliceGameMode gameMode;
                gameMode.OnLevelStart(&context);
                
                Entity foundPlayer = gameMode.FindPlayerEntity();
                if (foundPlayer == INVALID_ENTITY) {
                    LOG_ERROR("[Stress] Checkpoint Test 1 FAILED: GameMode could not find player entity!");
                    return false;
                }

                // Simulate player entering trigger
                GameplayEvent event;
                event.Type = GameplayEventType::TriggerEnter;
                event.Source = foundPlayer;
                event.Target = checkpoint;
                event.ObjectiveID = "CP_BUNKER";
                eventBus.Publish(event);
                eventBus.FlushEvents();

                // Verify CheckpointSystem triggered
                auto* cpSys = gameMode.GetCheckpointSystem();
                if (!cpSys->HasValidCheckpoint()) {
                    LOG_ERROR("[Stress] Checkpoint Test 1 FAILED: Checkpoint was not activated!");
                    return false;
                }

                if (gameMode.GetGameState().CurrentCheckpointID != "CP_BUNKER") {
                    LOG_ERROR("[Stress] Checkpoint Test 1 FAILED: GameState CurrentCheckpointID was not updated! Got: %s", gameMode.GetGameState().CurrentCheckpointID.c_str());
                    return false;
                }

                gameMode.OnLevelEnd();
            }

            // Test 2: Capture Player Transform
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                Entity player = world->CreateEntity();
                PlayerStateComponent psc;
                world->AddComponent<PlayerStateComponent>(player, psc);
                PlayerTagComponent tag;
                world->AddComponent<PlayerTagComponent>(player, tag);
                TransformComponent pTrans;
                pTrans.position = Vector3(10.0f, 20.0f, 30.0f);
                world->AddComponent<TransformComponent>(player, pTrans);

                Entity checkpoint = world->CreateEntity();
                CheckpointComponent cc;
                cc.CheckpointID = "CP_VAL";
                world->AddComponent<CheckpointComponent>(checkpoint, cc);

                VerticalSliceGameMode gameMode;
                gameMode.OnLevelStart(&context);

                // Approach checkpoint
                GameplayEvent event;
                event.Type = GameplayEventType::TriggerEnter;
                event.Source = player;
                event.Target = checkpoint;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                auto* cpSys = gameMode.GetCheckpointSystem();
                const auto& snap = cpSys->GetCurrentSnapshot();
                if (snap.PlayerTransform.position.x != 10.0f || snap.PlayerTransform.position.y != 20.0f) {
                    LOG_ERROR("[Stress] Checkpoint Test 2 FAILED: Player position was not captured correctly! Got: (%.2f, %.2f, %.2f)", snap.PlayerTransform.position.x, snap.PlayerTransform.position.y, snap.PlayerTransform.position.z);
                    return false;
                }

                gameMode.OnLevelEnd();
            }

            // Test 3: Capture Objective Progress
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                Entity player = world->CreateEntity();
                PlayerStateComponent psc;
                world->AddComponent<PlayerStateComponent>(player, psc);
                PlayerTagComponent tag;
                world->AddComponent<PlayerTagComponent>(player, tag);

                Entity checkpoint = world->CreateEntity();
                CheckpointComponent cc;
                cc.CheckpointID = "CP_OBJ";
                world->AddComponent<CheckpointComponent>(checkpoint, cc);

                // Create objective entity
                Entity objectiveEnt = world->CreateEntity();
                ObjectiveComponent objComp;
                objComp.ObjectiveID = "OBJ_FIND_KEY";
                objComp.Title = "Find Keycard";
                objComp.StartsActive = true;
                world->AddComponent<ObjectiveComponent>(objectiveEnt, objComp);

                VerticalSliceGameMode gameMode;
                gameMode.OnLevelStart(&context);

                // Complete objective
                gameMode.GetObjectiveSystem()->CompleteObjective("OBJ_FIND_KEY");

                // Start another
                gameMode.GetObjectiveSystem()->StartObjective("OBJ_OPEN_GATE");

                // Trigger checkpoint
                GameplayEvent event;
                event.Type = GameplayEventType::TriggerEnter;
                event.Source = player;
                event.Target = checkpoint;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                const auto& snap = gameMode.GetCheckpointSystem()->GetCurrentSnapshot();
                if (snap.ActiveObjectiveID != "OBJ_OPEN_GATE") {
                    LOG_ERROR("[Stress] Checkpoint Test 3 FAILED: Active objective not captured! Got: %s", snap.ActiveObjectiveID.c_str());
                    return false;
                }
                bool foundCompleted = std::find(snap.CompletedObjectives.begin(), snap.CompletedObjectives.end(), "OBJ_FIND_KEY") != snap.CompletedObjectives.end();
                if (!foundCompleted) {
                    LOG_ERROR("[Stress] Checkpoint Test 3 FAILED: Completed objectives list not captured correctly!");
                    return false;
                }

                gameMode.OnLevelEnd();
            }

            // Test 4: Capture Simple State Objects
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                Entity player = world->CreateEntity();
                PlayerStateComponent psc;
                world->AddComponent<PlayerStateComponent>(player, psc);
                PlayerTagComponent tag;
                world->AddComponent<PlayerTagComponent>(player, tag);

                // Create door object
                Entity doorObj = world->CreateEntity();
                SimpleStateComponent doorState;
                doorState.InitialState = SimpleObjectState::Completed;
                doorState.CurrentState = SimpleObjectState::Completed; // already opened
                world->AddComponent<SimpleStateComponent>(doorObj, doorState);

                ActivatableComponent doorAct;
                doorAct.ActivationID = "TEST_DOOR";
                world->AddComponent<ActivatableComponent>(doorObj, doorAct);

                DoorComponent doorComp;
                doorComp.OpenMode = DoorOpenMode::Instant;
                doorComp.ClosedPosition = Vector3(0.0f, 0.0f, 0.0f);
                doorComp.OpenOffset = Vector3(0.0f, 5.0f, 0.0f);
                world->AddComponent<DoorComponent>(doorObj, doorComp);

                TransformComponent doorTrans;
                world->AddComponent<TransformComponent>(doorObj, doorTrans);

                Entity checkpoint = world->CreateEntity();
                CheckpointComponent cc;
                cc.CheckpointID = "CP_DOOR";
                world->AddComponent<CheckpointComponent>(checkpoint, cc);

                VerticalSliceGameMode gameMode;
                gameMode.OnLevelStart(&context);

                // Set position to open position to mimic open door
                auto& coordinator = world->getCoordinator();
                coordinator.GetComponent<TransformComponent>(doorObj).position = Vector3(0.0f, 5.0f, 0.0f);

                // Trigger checkpoint
                GameplayEvent event;
                event.Type = GameplayEventType::TriggerEnter;
                event.Source = player;
                event.Target = checkpoint;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                const auto& snap = gameMode.GetCheckpointSystem()->GetCurrentSnapshot();
                auto it = snap.SimpleObjectStates.find("TEST_DOOR");
                if (it == snap.SimpleObjectStates.end() || it->second != SimpleObjectState::Completed) {
                    LOG_ERROR("[Stress] Checkpoint Test 4 FAILED: Door state was not captured in snapshot!");
                    return false;
                }

                gameMode.OnLevelEnd();
            }

            // Test 5: One-Shot vs Repeatable Checkpoints
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                Entity player = world->CreateEntity();
                PlayerStateComponent psc;
                world->AddComponent<PlayerStateComponent>(player, psc);
                PlayerTagComponent tag;
                world->AddComponent<PlayerTagComponent>(player, tag);
                TransformComponent pTrans;
                world->AddComponent<TransformComponent>(player, pTrans);

                Entity cpRepeatable = world->CreateEntity();
                CheckpointComponent cc1;
                cc1.CheckpointID = "CP_REPEAT";
                cc1.OneShot = false;
                world->AddComponent<CheckpointComponent>(cpRepeatable, cc1);

                Entity cpOneShot = world->CreateEntity();
                CheckpointComponent cc2;
                cc2.CheckpointID = "CP_ONESHOT";
                cc2.OneShot = true;
                world->AddComponent<CheckpointComponent>(cpOneShot, cc2);

                VerticalSliceGameMode gameMode;
                gameMode.OnLevelStart(&context);

                auto& coordinator = world->getCoordinator();

                // Trigger repeatable first time
                coordinator.GetComponent<TransformComponent>(player).position = Vector3(1.0f, 1.0f, 1.0f);
                GameplayEvent event1;
                event1.Type = GameplayEventType::TriggerEnter;
                event1.Source = player;
                event1.Target = cpRepeatable;
                eventBus.Publish(event1);
                eventBus.FlushEvents();

                auto* cpSys = gameMode.GetCheckpointSystem();
                if (cpSys->GetCurrentSnapshot().PlayerTransform.position.x != 1.0f) {
                    LOG_ERROR("[Stress] Checkpoint Test 5 FAILED: Repeatable capture 1 failed!");
                    return false;
                }

                // Trigger repeatable second time with new position
                coordinator.GetComponent<TransformComponent>(player).position = Vector3(2.0f, 2.0f, 2.0f);
                eventBus.Publish(event1);
                eventBus.FlushEvents();

                if (cpSys->GetCurrentSnapshot().PlayerTransform.position.x != 2.0f) {
                    LOG_ERROR("[Stress] Checkpoint Test 5 FAILED: Repeatable capture 2 failed to overwrite!");
                    return false;
                }

                // Trigger one-shot first time
                coordinator.GetComponent<TransformComponent>(player).position = Vector3(10.0f, 10.0f, 10.0f);
                GameplayEvent event2;
                event2.Type = GameplayEventType::TriggerEnter;
                event2.Source = player;
                event2.Target = cpOneShot;
                eventBus.Publish(event2);
                eventBus.FlushEvents();

                if (cpSys->GetCurrentSnapshot().PlayerTransform.position.x != 10.0f) {
                    LOG_ERROR("[Stress] Checkpoint Test 5 FAILED: One-shot capture 1 failed!");
                    return false;
                }

                // Trigger one-shot second time with new position (should NOT update!)
                coordinator.GetComponent<TransformComponent>(player).position = Vector3(20.0f, 20.0f, 20.0f);
                eventBus.Publish(event2);
                eventBus.FlushEvents();

                if (cpSys->GetCurrentSnapshot().PlayerTransform.position.x == 20.0f) {
                    LOG_ERROR("[Stress] Checkpoint Test 5 FAILED: One-shot capture 2 updated snapshot instead of being ignored!");
                    return false;
                }

                gameMode.OnLevelEnd();
            }

            // Test 6: Checkpoint Restart & Restore
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                context.mode = RuntimeMode::Game;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                auto& coordinator = world->getCoordinator();

                // Player
                Entity player = world->CreateEntity();
                PlayerStateComponent psc;
                psc.Health = 100.0f;
                psc.IsAlive = true;
                world->AddComponent<PlayerStateComponent>(player, psc);
                PlayerTagComponent tag;
                world->AddComponent<PlayerTagComponent>(player, tag);
                TransformComponent pTrans;
                pTrans.position = Vector3(5.0f, 5.0f, 5.0f);
                world->AddComponent<TransformComponent>(player, pTrans);

                // Door
                Entity doorObj = world->CreateEntity();
                SimpleStateComponent doorState;
                doorState.InitialState = SimpleObjectState::Completed;
                doorState.CurrentState = SimpleObjectState::Completed;
                world->AddComponent<SimpleStateComponent>(doorObj, doorState);

                ActivatableComponent doorAct;
                doorAct.ActivationID = "DOOR_RESTORE";
                world->AddComponent<ActivatableComponent>(doorObj, doorAct);

                DoorComponent doorComp;
                doorComp.OpenMode = DoorOpenMode::Instant;
                doorComp.ClosedPosition = Vector3(0.0f, 0.0f, 0.0f);
                doorComp.OpenOffset = Vector3(0.0f, 5.0f, 0.0f);
                doorComp.IsOpen = true;
                world->AddComponent<DoorComponent>(doorObj, doorComp);

                TransformComponent doorTrans;
                world->AddComponent<TransformComponent>(doorObj, doorTrans);

                // Checkpoint
                Entity checkpoint = world->CreateEntity();
                CheckpointComponent cc;
                cc.CheckpointID = "CP_RESTORE";
                world->AddComponent<CheckpointComponent>(checkpoint, cc);

                VerticalSliceGameMode gameMode;
                gameMode.OnLevelStart(&context);

                coordinator.GetComponent<TransformComponent>(doorObj).position = Vector3(0.0f, 5.0f, 0.0f);

                // Trigger checkpoint
                GameplayEvent event;
                event.Type = GameplayEventType::TriggerEnter;
                event.Source = player;
                event.Target = checkpoint;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                // Mutate state after checkpoint (player moved, dies, door resets)
                coordinator.GetComponent<TransformComponent>(player).position = Vector3(100.0f, 100.0f, 100.0f);
                coordinator.GetComponent<PlayerStateComponent>(player).Health = 0.0f;
                coordinator.GetComponent<PlayerStateComponent>(player).IsAlive = false;

                coordinator.GetComponent<TransformComponent>(doorObj).position = Vector3(0.0f, 0.0f, 0.0f);
                coordinator.GetComponent<SimpleStateComponent>(doorObj).CurrentState = SimpleObjectState::Locked;
                coordinator.GetComponent<DoorComponent>(doorObj).IsOpen = false;

                // Restart from checkpoint
                gameMode.RestartLevel();

                // Verify restoration
                const auto& playerTransAfter = coordinator.GetComponent<TransformComponent>(player);
                const auto& playerStateAfter = coordinator.GetComponent<PlayerStateComponent>(player);
                const auto& doorTransAfter = coordinator.GetComponent<TransformComponent>(doorObj);
                const auto& doorStateAfter = coordinator.GetComponent<SimpleStateComponent>(doorObj);
                const auto& doorCompAfter = coordinator.GetComponent<DoorComponent>(doorObj);

                if (playerTransAfter.position.x != 5.0f || playerTransAfter.position.y != 5.0f) {
                    LOG_ERROR("[Stress] Checkpoint Test 6 FAILED: Player position was not restored! Got: (%.2f, %.2f)", playerTransAfter.position.x, playerTransAfter.position.y);
                    return false;
                }

                if (!playerStateAfter.IsAlive || playerStateAfter.Health != playerStateAfter.MaxHealth) {
                    LOG_ERROR("[Stress] Checkpoint Test 6 FAILED: Player health/alive state not restored!");
                    return false;
                }

                if (doorTransAfter.position.y != 5.0f || doorStateAfter.CurrentState != SimpleObjectState::Completed || !doorCompAfter.IsOpen) {
                    LOG_ERROR("[Stress] Checkpoint Test 6 FAILED: Simple state object door was not restored to completed/open! "
                              "Got: position.y = %.2f, CurrentState = %d, IsOpen = %d",
                              doorTransAfter.position.y, (int)doorStateAfter.CurrentState, (int)doorCompAfter.IsOpen);
                    return false;
                }

                gameMode.OnLevelEnd();
            }

            // Test 7: Play/Edit Isolation
            {
                RuntimeContext context;
                GameplayEventBus eventBus;
                context.gameplayEventBus = &eventBus;
                
                // Set to Edit Mode
                context.mode = RuntimeMode::Editor;
                context.editorSimulationState = EditorSimulationState::Edit;

                auto world = std::make_unique<World>();
                world->Initialize();
                context.ecs = world.get();

                Entity player = world->CreateEntity();
                PlayerStateComponent psc;
                world->AddComponent<PlayerStateComponent>(player, psc);
                PlayerTagComponent tag;
                world->AddComponent<PlayerTagComponent>(player, tag);
                TransformComponent pTrans;
                world->AddComponent<TransformComponent>(player, pTrans);

                Entity checkpoint = world->CreateEntity();
                CheckpointComponent cc;
                cc.CheckpointID = "CP_EDIT";
                world->AddComponent<CheckpointComponent>(checkpoint, cc);

                VerticalSliceGameMode gameMode;
                gameMode.OnLevelStart(&context);

                // Trigger in edit mode
                GameplayEvent event;
                event.Type = GameplayEventType::TriggerEnter;
                event.Source = player;
                event.Target = checkpoint;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                auto* cpSys = gameMode.GetCheckpointSystem();
                if (cpSys->HasValidCheckpoint()) {
                    LOG_ERROR("[Stress] Checkpoint Test 7 FAILED: Checkpoint triggered during Edit Mode!");
                    return false;
                }

                // Transition to Play and trigger
                context.editorSimulationState = EditorSimulationState::Play;
                eventBus.Publish(event);
                eventBus.FlushEvents();

                if (!cpSys->HasValidCheckpoint()) {
                    LOG_ERROR("[Stress] Checkpoint Test 7 FAILED: Checkpoint failed to trigger in Play Mode!");
                    return false;
                }

                gameMode.OnLevelEnd();
            }

            LOG_INFO("[Stress] Checkpoint System Integration Test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // 11. GAMEPLAY SAVE SYSTEM INTEGRATION TEST
        // -----------------------------------------------------------------------------
        {
            using namespace eng::runtime;
            LOG_INFO("[Stress] Starting Gameplay Save System Integration Test...");

            // Setup common test context
            RuntimeContext context;
            GameplayEventBus eventBus;
            context.gameplayEventBus = &eventBus;
            context.mode = RuntimeMode::Game;

            auto world = std::make_unique<World>();
            world->Initialize();
            context.ecs = world.get();

            SceneManager sceneMgr(&world->getCoordinator());
            context.scenes = &sceneMgr;
            sceneMgr.CreateNewScene("test_room");

            // Create player entity
            Entity player = world->CreateEntity();
            
            PlayerStateComponent psc;
            psc.Health = 80.0f;
            psc.IsAlive = true;
            psc.ActivePlayer = player;
            world->AddComponent<PlayerStateComponent>(player, psc);
            
            PlayerTagComponent tag;
            world->AddComponent<PlayerTagComponent>(player, tag);
            
            TransformComponent pTrans;
            pTrans.position = Vector3(10.0f, 0.0f, 5.0f);
            world->AddComponent<TransformComponent>(player, pTrans);

            // Instantiate GameplaySaveSystem
            GameplaySaveSystem saveSystem;
            saveSystem.Initialize(&context);
            context.saveSystem = &saveSystem;

            // Instantiate GameMode
            VerticalSliceGameMode gameMode;
            gameMode.OnLevelStart(&context);
            
            // Setup initial GameState parameters
            gameMode.GetGameStateMutable().ActiveObjectiveID = "OBJ_OPEN_GATE";
            gameMode.GetGameStateMutable().CurrentCheckpointID = "CP_ENTRANCE";
            gameMode.GetGameStateMutable().CompletedObjectives.clear(); // Ensure clean start

            // Setup Door A and Terminal A
            Entity doorObj = world->CreateEntity();
            SimpleStateComponent doorState;
            doorState.CurrentState = SimpleObjectState::Completed;
            world->AddComponent<SimpleStateComponent>(doorObj, doorState);

            ActivatableComponent doorAct;
            doorAct.ActivationID = "Door_A";
            doorAct.HasActivated = false;
            world->AddComponent<ActivatableComponent>(doorObj, doorAct);

            DoorComponent doorComp;
            doorComp.ClosedPosition = Vector3(0.0f, 0.0f, 0.0f);
            doorComp.OpenOffset = Vector3(0.0f, 5.0f, 0.0f);
            doorComp.IsOpen = true;
            world->AddComponent<DoorComponent>(doorObj, doorComp);

            TransformComponent doorTrans;
            doorTrans.position = Vector3(0.0f, 5.0f, 0.0f);
            world->AddComponent<TransformComponent>(doorObj, doorTrans);

            Entity terminalObj = world->CreateEntity();
            SimpleStateComponent termState;
            termState.CurrentState = SimpleObjectState::Active;
            world->AddComponent<SimpleStateComponent>(terminalObj, termState);

            ActivatableComponent termAct;
            termAct.ActivationID = "Terminal_A";
            termAct.HasActivated = true;
            world->AddComponent<ActivatableComponent>(terminalObj, termAct);

            // Test 1: Basic Save File Creation
            {
                GameplaySaveSnapshot snapshot = saveSystem.CaptureSnapshot();
                if (!snapshot.Valid) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Captured snapshot is invalid!");
                    return false;
                }
                if (snapshot.SceneName != "test_room") {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Snapshot SceneName is wrong: %s", snapshot.SceneName.c_str());
                    return false;
                }

                std::string savePath = "Saves/autosave_01.omnixsave";
                if (!saveSystem.SaveToFile(savePath, snapshot)) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: SaveToFile returned false!");
                    return false;
                }

                // Verify file exists and has correct header fields
                std::ifstream is(savePath, std::ios::in | std::ios::binary);
                if (!is.is_open()) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Save file was not created on disk!");
                    return false;
                }

                GameplaySaveHeader header;
                is.read(reinterpret_cast<char*>(&header), sizeof(GameplaySaveHeader));
                if (is.fail()) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Could not read header from saved file!");
                    return false;
                }

                if (std::memcmp(header.Magic, "OMNSAVE\0", 8) != 0) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Magic header mismatch!");
                    return false;
                }

                if (header.Version != 1) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Saved version is wrong: %u", header.Version);
                    return false;
                }

                if (header.PayloadSize == 0) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Payload size is zero!");
                    return false;
                }

                if (header.Checksum != snapshot.Checksum) {
                    LOG_ERROR("[Stress] Save Test 1 FAILED: Checksum mismatch in header!");
                    return false;
                }
                is.close();
            }

            // Test 2: Save / Restore Player State
            {
                GameplaySaveSnapshot snapshot;
                if (!saveSystem.LoadFromFile("Saves/autosave_01.omnixsave", snapshot)) {
                    LOG_ERROR("[Stress] Save Test 2 FAILED: LoadFromFile returned false!");
                    return false;
                }

                // Mutate current live state
                auto& coordinator = world->getCoordinator();
                coordinator.GetComponent<TransformComponent>(player).position = Vector3(100.0f, 0.0f, 100.0f);
                coordinator.GetComponent<PlayerStateComponent>(player).Health = 10.0f;
                coordinator.GetComponent<PlayerStateComponent>(player).IsAlive = true;

                // Restore from save
                if (!gameMode.RestoreFromSnapshot(snapshot)) {
                    LOG_ERROR("[Stress] Save Test 2 FAILED: RestoreFromSnapshot returned false!");
                    return false;
                }

                // Verify player state is restored
                const auto& restoredTrans = coordinator.GetComponent<TransformComponent>(player);
                const auto& restoredPsc = coordinator.GetComponent<PlayerStateComponent>(player);
                if (restoredTrans.position.x != 10.0f || restoredTrans.position.z != 5.0f) {
                    LOG_ERROR("[Stress] Save Test 2 FAILED: Player position not restored! Got: (%.2f, %.2f)", restoredTrans.position.x, restoredTrans.position.z);
                    return false;
                }
                if (restoredPsc.Health != 80.0f || !restoredPsc.IsAlive) {
                    LOG_ERROR("[Stress] Save Test 2 FAILED: Player health/alive status not restored! Health: %.1f", restoredPsc.Health);
                    return false;
                }
            }

            // Test 3: Save / Restore Objective State
            {
                // Setup objective state
                gameMode.GetGameStateMutable().ActiveObjectiveID = "OBJ_ESCAPE";
                gameMode.GetGameStateMutable().CompletedObjectives = {"OBJ_FIND_KEY", "OBJ_OPEN_DOOR"};

                // Register objectives dynamically to support verification
                auto* objSys = gameMode.GetObjectiveSystem();
                if (objSys) {
                    objSys->StartObjective("OBJ_ESCAPE");
                    objSys->CompleteObjective("OBJ_FIND_KEY");
                    objSys->CompleteObjective("OBJ_OPEN_DOOR");
                }

                GameplaySaveSnapshot snapshot = saveSystem.CaptureSnapshot();
                saveSystem.SaveToFile("Saves/autosave_01.omnixsave", snapshot);

                // Mutate live objective state
                gameMode.GetGameStateMutable().ActiveObjectiveID = "OBJ_WRONG";
                gameMode.GetGameStateMutable().CompletedObjectives.clear();
                if (objSys) {
                    objSys->RestoreObjectiveState("OBJ_WRONG", {});
                }

                // Load & restore
                GameplaySaveSnapshot loaded;
                saveSystem.LoadFromFile("Saves/autosave_01.omnixsave", loaded);
                gameMode.RestoreFromSnapshot(loaded);

                // Verify objective state restored
                const auto& restoredGs = gameMode.GetGameState();
                if (restoredGs.ActiveObjectiveID != "OBJ_ESCAPE") {
                    LOG_ERROR("[Stress] Save Test 3 FAILED: ActiveObjectiveID not restored! Got: %s", restoredGs.ActiveObjectiveID.c_str());
                    return false;
                }
                if (restoredGs.CompletedObjectives.size() != 2 || 
                    restoredGs.CompletedObjectives[0] != "OBJ_FIND_KEY" || 
                    restoredGs.CompletedObjectives[1] != "OBJ_OPEN_DOOR") {
                    LOG_ERROR("[Stress] Save Test 3 FAILED: Completed objectives not restored correctly!");
                    return false;
                }
            }

            // Test 4: Save / Restore Simple Object States
            {
                // Setup states
                auto& coordinator = world->getCoordinator();
                coordinator.GetComponent<SimpleStateComponent>(doorObj).CurrentState = SimpleObjectState::Completed;
                coordinator.GetComponent<SimpleStateComponent>(terminalObj).CurrentState = SimpleObjectState::Locked;
                coordinator.GetComponent<ActivatableComponent>(terminalObj).HasActivated = false;

                GameplaySaveSnapshot snapshot = saveSystem.CaptureSnapshot();
                saveSystem.SaveToFile("Saves/autosave_01.omnixsave", snapshot);

                // Mutate
                coordinator.GetComponent<SimpleStateComponent>(doorObj).CurrentState = SimpleObjectState::Locked;
                coordinator.GetComponent<SimpleStateComponent>(terminalObj).CurrentState = SimpleObjectState::Completed;
                coordinator.GetComponent<ActivatableComponent>(terminalObj).HasActivated = true;

                // Load & restore
                GameplaySaveSnapshot loaded;
                saveSystem.LoadFromFile("Saves/autosave_01.omnixsave", loaded);
                gameMode.RestoreFromSnapshot(loaded);

                // Verify
                const auto& restoredDoorState = coordinator.GetComponent<SimpleStateComponent>(doorObj);
                const auto& restoredTermState = coordinator.GetComponent<SimpleStateComponent>(terminalObj);
                const auto& restoredTermAct = coordinator.GetComponent<ActivatableComponent>(terminalObj);

                if (restoredDoorState.CurrentState != SimpleObjectState::Completed) {
                    LOG_ERROR("[Stress] Save Test 4 FAILED: Door state not restored!");
                    return false;
                }
                if (restoredTermState.CurrentState != SimpleObjectState::Locked) {
                    LOG_ERROR("[Stress] Save Test 4 FAILED: Terminal state not restored!");
                    return false;
                }
                if (restoredTermAct.HasActivated != false) {
                    LOG_ERROR("[Stress] Save Test 4 FAILED: Terminal activation status not restored!");
                    return false;
                }
            }

            // Test 5: Deterministic Save Output
            {
                GameplaySaveSnapshot snapA = saveSystem.CaptureSnapshot();
                GameplaySaveSnapshot snapB = saveSystem.CaptureSnapshot();

                saveSystem.SaveToFile("Saves/save_a.omnixsave", snapA);
                saveSystem.SaveToFile("Saves/save_b.omnixsave", snapB);

                // Verify byte-by-byte identity
                std::ifstream fileA("Saves/save_a.omnixsave", std::ios::in | std::ios::binary | std::ios::ate);
                std::ifstream fileB("Saves/save_b.omnixsave", std::ios::in | std::ios::binary | std::ios::ate);

                if (fileA.tellg() != fileB.tellg()) {
                    LOG_ERROR("[Stress] Save Test 5 FAILED: Save files have different sizes!");
                    return false;
                }

                size_t size = fileA.tellg();
                fileA.seekg(0, std::ios::beg);
                fileB.seekg(0, std::ios::beg);

                std::vector<char> bufferA(size);
                std::vector<char> bufferB(size);

                fileA.read(bufferA.data(), size);
                fileB.read(bufferB.data(), size);

                if (std::memcmp(bufferA.data(), bufferB.data(), size) != 0) {
                    LOG_ERROR("[Stress] Save Test 5 FAILED: Binary mismatch between save_a and save_b!");
                    return false;
                }

                if (snapA.Checksum != snapB.Checksum) {
                    LOG_ERROR("[Stress] Save Test 5 FAILED: Checksums do not match for identical states!");
                    return false;
                }
            }

            // Test 6: Corrupted Save Rejection
            {
                // Create valid save
                GameplaySaveSnapshot snapshot = saveSystem.CaptureSnapshot();
                saveSystem.SaveToFile("Saves/autosave_01.omnixsave", snapshot);

                // Read valid save, modify bytes, write back corrupted save
                std::string savePath = "Saves/autosave_01.omnixsave";
                std::fstream file(savePath, std::ios::in | std::ios::out | std::ios::binary);
                if (file.is_open()) {
                    // Mutate a byte in the payload area (offset by 30 bytes)
                    file.seekp(30, std::ios::beg);
                    char badByte = 0x5F;
                    file.write(&badByte, 1);
                    file.close();
                }

                // Mutate live state before load attempt
                auto& coordinator = world->getCoordinator();
                coordinator.GetComponent<TransformComponent>(player).position = Vector3(99.0f, 99.0f, 99.0f);

                // Try to load corrupted save — should fail
                GameplaySaveSnapshot loaded;
                bool success = saveSystem.LoadFromFile(savePath, loaded);

                if (success) {
                    LOG_ERROR("[Stress] Save Test 6 FAILED: Corrupted save was loaded successfully (should have failed checksum)!");
                    return false;
                }

                // Verify live state remains unchanged (not restored to old position)
                const auto& livePos = coordinator.GetComponent<TransformComponent>(player).position;
                if (livePos.x != 99.0f) {
                    LOG_ERROR("[Stress] Save Test 6 FAILED: Live state was partially mutated on load failure!");
                    return false;
                }
            }

            // Test 7: Edit Mode Isolation
            {
                // Edit Mode scene contains closed door (Locked)
                context.mode = RuntimeMode::Editor;
                context.editorSimulationState = EditorSimulationState::Edit;

                auto& coordinator = world->getCoordinator();
                coordinator.GetComponent<SimpleStateComponent>(doorObj).CurrentState = SimpleObjectState::Locked;

                // Transition to Play simulation mode
                context.editorSimulationState = EditorSimulationState::Play;
                coordinator.GetComponent<SimpleStateComponent>(doorObj).CurrentState = SimpleObjectState::Completed;

                GameplaySaveSnapshot snapshot = saveSystem.CaptureSnapshot();
                saveSystem.SaveToFile("Saves/autosave_01.omnixsave", snapshot);

                // Restore in play mode
                gameMode.RestoreFromSnapshot(snapshot);

                // Verify play state is Completed
                if (coordinator.GetComponent<SimpleStateComponent>(doorObj).CurrentState != SimpleObjectState::Completed) {
                    LOG_ERROR("[Stress] Save Test 7 FAILED: Play state door is not Completed!");
                    return false;
                }

                // Transition back to Edit simulation state (stops Play)
                context.editorSimulationState = EditorSimulationState::Edit;
            }

            gameMode.OnLevelEnd();
            LOG_INFO("[Stress] Gameplay Save System Integration Test passed successfully.");
        }

        // -----------------------------------------------------------------------------
        // 11. GAMEPLAY VALIDATION SYSTEM INTEGRATION TESTS
        // -----------------------------------------------------------------------------
        LOG_INFO("[Stress] Starting Gameplay Validation System Integration Tests...");
        {
            eng::runtime::GameplayValidator validator;

            // Test 1: Missing PlayerStart (Fatal)
            {
                Scene scene("TestScene_NoPlayerStart");
                
                auto results = validator.ValidateScene(scene);
                if (!validator.HasFatalErrors(results)) {
                    LOG_ERROR("[Stress] Validation Test 1 FAILED: Scene without PlayerStart did not report Fatal errors!");
                    return false;
                }
                
                bool foundPlayerStartFatal = false;
                for (const auto& r : results) {
                    if (r.Severity == eng::runtime::ValidationSeverity::Fatal && 
                        r.Message.find("No PlayerStart") != std::string::npos) {
                        foundPlayerStartFatal = true;
                        break;
                    }
                }
                if (!foundPlayerStartFatal) {
                    LOG_ERROR("[Stress] Validation Test 1 FAILED: Could not find Fatal error for missing PlayerStart!");
                    return false;
                }
            }

            // Test 2: Duplicate Objective IDs (Fatal)
            {
                Scene scene("TestScene_DuplicateObjectives");
                
                auto playerStartObj = std::make_shared<SceneObject>("PlayerStart");
                PlayerStartComponent psc;
                psc.active = true;
                playerStartObj->SetPlayerStart(psc);
                scene.AddSceneObject(playerStartObj);

                auto obj1 = std::make_shared<SceneObject>("Obj1");
                ObjectiveComponent oc1;
                oc1.ObjectiveID = "DUPLICATE_ID";
                oc1.StartsActive = true;
                obj1->SetObjective(oc1);
                scene.AddSceneObject(obj1);

                auto obj2 = std::make_shared<SceneObject>("Obj2");
                ObjectiveComponent oc2;
                oc2.ObjectiveID = "DUPLICATE_ID";
                obj2->SetObjective(oc2);
                scene.AddSceneObject(obj2);

                auto results = validator.ValidateScene(scene);
                if (!validator.HasFatalErrors(results)) {
                    LOG_ERROR("[Stress] Validation Test 2 FAILED: Duplicate Objective IDs did not report Fatal errors!");
                    return false;
                }
                
                bool foundDuplicateFatal = false;
                for (const auto& r : results) {
                    if (r.Severity == eng::runtime::ValidationSeverity::Fatal && 
                        r.Message.find("Duplicate Objective ID") != std::string::npos) {
                        foundDuplicateFatal = true;
                        break;
                    }
                }
                if (!foundDuplicateFatal) {
                    LOG_ERROR("[Stress] Validation Test 2 FAILED: Could not find Fatal error for duplicate Objective IDs!");
                    return false;
                }
            }

            // Test 3: Missing Objective Components (Error)
            {
                Scene scene("TestScene_MissingObjectiveComps");
                
                auto playerStartObj = std::make_shared<SceneObject>("PlayerStart");
                PlayerStartComponent psc;
                psc.active = true;
                playerStartObj->SetPlayerStart(psc);
                scene.AddSceneObject(playerStartObj);

                // Objective completion mode is Interaction, but no Interactable component exists
                auto obj1 = std::make_shared<SceneObject>("Obj1");
                ObjectiveComponent oc1;
                oc1.ObjectiveID = "OBJ_INTERACTION";
                oc1.CompletionMode = ObjectiveCompletionMode::Interaction;
                oc1.StartsActive = true;
                obj1->SetObjective(oc1);
                scene.AddSceneObject(obj1);

                // Objective completion mode is TriggerEnter, but no Trigger component exists
                auto obj2 = std::make_shared<SceneObject>("Obj2");
                ObjectiveComponent oc2;
                oc2.ObjectiveID = "OBJ_TRIGGER";
                oc2.CompletionMode = ObjectiveCompletionMode::TriggerEnter;
                obj2->SetObjective(oc2);
                scene.AddSceneObject(obj2);

                auto results = validator.ValidateScene(scene);
                
                bool foundInteractionError = false;
                bool foundTriggerError = false;
                for (const auto& r : results) {
                    if (r.Severity == eng::runtime::ValidationSeverity::Error) {
                        if (r.Message.find("requires Interaction but entity has no InteractableComponent") != std::string::npos) {
                            foundInteractionError = true;
                        }
                        if (r.Message.find("requires TriggerEnter but entity has no trigger component") != std::string::npos) {
                            foundTriggerError = true;
                        }
                    }
                }
                if (!foundInteractionError || !foundTriggerError) {
                    LOG_ERROR("[Stress] Validation Test 3 FAILED: Missing objective interaction/trigger component errors were not caught!");
                    return false;
                }
            }

            // Test 4: Activatable Graph Matching (Error)
            {
                Scene scene("TestScene_BrokenActivatable");
                
                auto playerStartObj = std::make_shared<SceneObject>("PlayerStart");
                PlayerStartComponent psc;
                psc.active = true;
                playerStartObj->SetPlayerStart(psc);
                scene.AddSceneObject(playerStartObj);

                auto actObj = std::make_shared<SceneObject>("Activatable");
                ActivatableComponent ac;
                ac.ActivationID = "ACT_001";
                ac.TargetActivationID = "NON_EXISTENT_TARGET"; // broken graph
                actObj->SetActivatable(ac);
                scene.AddSceneObject(actObj);

                auto results = validator.ValidateScene(scene);
                
                bool foundBrokenTargetError = false;
                for (const auto& r : results) {
                    if (r.Severity == eng::runtime::ValidationSeverity::Error && 
                        r.Message.find("targets activation ID 'NON_EXISTENT_TARGET', but no object owns that ID") != std::string::npos) {
                        foundBrokenTargetError = true;
                        break;
                    }
                }
                if (!foundBrokenTargetError) {
                    LOG_ERROR("[Stress] Validation Test 4 FAILED: Broken activatable target was not flagged as Error!");
                    return false;
                }
            }

            // Test 5: Door Component Safeguards (Error)
            {
                Scene scene("TestScene_DoorSafeguards");
                
                auto playerStartObj = std::make_shared<SceneObject>("PlayerStart");
                PlayerStartComponent psc;
                psc.active = true;
                playerStartObj->SetPlayerStart(psc);
                scene.AddSceneObject(playerStartObj);

                // Door missing SimpleState component
                auto door1 = std::make_shared<SceneObject>("Door1");
                DoorComponent dc1;
                dc1.OpenMode = DoorOpenMode::Instant;
                door1->SetDoor(dc1);
                scene.AddSceneObject(door1);

                // Smooth door with OpenSpeed = 0
                auto door2 = std::make_shared<SceneObject>("Door2");
                SimpleStateComponent ssc2;
                door2->SetSimpleState(ssc2);
                DoorComponent dc2;
                dc2.OpenMode = DoorOpenMode::Smooth;
                dc2.OpenSpeed = 0.0f;
                door2->SetDoor(dc2);
                scene.AddSceneObject(door2);

                auto results = validator.ValidateScene(scene);
                
                bool foundMissingStateError = false;
                bool foundZeroSpeedError = false;
                for (const auto& r : results) {
                    if (r.Severity == eng::runtime::ValidationSeverity::Error) {
                        if (r.Message.find("SimpleStateComponent is missing") != std::string::npos) {
                            foundMissingStateError = true;
                        }
                        if (r.Message.find("smooth mode has OpenSpeed = 0") != std::string::npos) {
                            foundZeroSpeedError = true;
                        }
                    }
                }
                if (!foundMissingStateError || !foundZeroSpeedError) {
                    LOG_ERROR("[Stress] Validation Test 5 FAILED: Door safeguards validation did not trigger correct errors!");
                    return false;
                }
            }

            // Test 6: Play Mode Block Simulation
            {
                Scene scene("TestScene_FatalPlayBlock");
                
                auto results = validator.ValidateScene(scene);
                if (!validator.HasFatalErrors(results)) {
                    LOG_ERROR("[Stress] Validation Test 6 FAILED: Expected fatal errors on empty scene, but got none!");
                    return false;
                }
                
                bool wouldBlock = validator.HasFatalErrors(results);
                if (!wouldBlock) {
                    LOG_ERROR("[Stress] Validation Test 6 FAILED: Gating logic failed to recognize fatal error block requirement!");
                    return false;
                }
            }

            // Test 7: Clean Pass Verification (test_room_gameplay.omnixscene)
            {
                Scene* loadedScene = SceneLoader::LoadFromFile("Assets/Scenes/test_room_gameplay.omnixscene");
                if (!loadedScene) {
                    LOG_ERROR("[Stress] Validation Test 7 FAILED: Could not load Assets/Scenes/test_room_gameplay.omnixscene!");
                    return false;
                }

                auto results = validator.ValidateScene(*loadedScene);
                
                bool hasErrors = false;
                for (const auto& r : results) {
                    if (r.Severity == eng::runtime::ValidationSeverity::Fatal || 
                        r.Severity == eng::runtime::ValidationSeverity::Error) {
                        LOG_ERROR("[Stress] Validation Test 7 FAILED: Clean scene reported error/fatal: %s", r.Message.c_str());
                        hasErrors = true;
                    }
                }
                
                delete loadedScene;
                if (hasErrors) {
                    return false;
                }
            }
        }
        LOG_INFO("[Stress] Gameplay Validation System Integration Tests passed successfully.");

        // -----------------------------------------------------------------------------
        // 12. MEMORY LEAK CHECK
        // -----------------------------------------------------------------------------
        LOG_INFO("[Stress] Running memory leak checks...");
        eng::memory::AllocationTracker::DumpLeakReport();
        size_t finalAllocations = eng::memory::AllocationTracker::GetActiveAllocationsCount();
        LOG_INFO("[Stress] Final active tracked memory allocations: %zu", finalAllocations);

        if (finalAllocations > initialAllocations) {
            LOG_ERROR("[Stress] Memory Leak Check FAILED: %zu allocations leaked during stress test execution!",
                      finalAllocations - initialAllocations);
            return false;
        }

        LOG_INFO("================================================================================");
        LOG_INFO("                  ALL RUNTIME INTEGRATION STRESS TESTS PASSED                    ");
        LOG_INFO("================================================================================");
        return true;
    }

} // namespace eng::diagnostics
