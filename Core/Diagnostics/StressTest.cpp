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
        // 4. MEMORY LEAK CHECK
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
