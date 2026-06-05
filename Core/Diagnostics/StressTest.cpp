#include "StressTest.h"
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
#include "Runtime/Public/Gameplay/VerticalSliceGameMode.h"
#include "Runtime/Public/Gameplay/PlayerStateComponent.h"
#include "Runtime/Public/Gameplay/Components/InteractableComponent.h"
#include "Runtime/Public/Gameplay/Systems/InteractionSystem.h"
#include "Runtime/Public/Gameplay/Components/ObjectiveComponent.h"
#include "Runtime/Public/Gameplay/Objectives/ObjectiveSystem.h"
#include "Runtime/Public/Gameplay/UI/GameplayHUD.h"
#include "Input/InputManager.h"
#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Gameplay/GameState.h"
#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
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
        // 8. MEMORY LEAK CHECK
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
