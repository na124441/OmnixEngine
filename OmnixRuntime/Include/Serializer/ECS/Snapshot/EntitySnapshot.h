#pragma once

#include <unordered_map>
#include <cstdint>
#include <vector>
#include <string>
#include "Serializer/ECS/Snapshot/ComponentSnapshot.h"
#include "../ECS.h"
#include "../SchemaRegistry.h"
#include "Serializer/ECS/Snapshot/SnapshotContext.h"
#include "../../Serialization/Normal/Binary/BinaryWriter.h" // Include BinaryWriter
#include "../../Serialization/Normal/Binary/BinaryReader.h" // Include BinaryReader

class EntitySnapshot {
private:
    uint32_t m_EntityID;
    uint32_t m_EntityGeneration;
    std::unordered_map<uint32_t, ComponentSnapshot> m_ComponentSnapshots;
    const SnapshotContext* m_Context = nullptr;
    bool m_IsValid = false;
    bool m_HasCapturedData = false;
    size_t m_ComponentCount = 0;
    const ComponentSchemaRegistry* m_SchemaRegistry = nullptr;

public:
    EntitySnapshot() : m_EntityID(0), m_EntityGeneration(0) {}
    explicit EntitySnapshot(uint32_t entityID, uint32_t generation = 0)
        : m_EntityID(entityID), m_EntityGeneration(generation) {}

    bool CaptureEntity(const ECS& ecsManager, const ComponentSchemaRegistry& registry, const SnapshotContext& context) {
        m_Context = &context;
        m_SchemaRegistry = &registry;
        const auto* entityMetadata = ecsManager.GetEntityMetadata(m_EntityID);
        if (!entityMetadata) {
            m_IsValid = false;
            m_HasCapturedData = false;
            return false;
        }

        m_ComponentSnapshots.clear(); // Clear previous snapshots

        for (const auto& componentTypeID : entityMetadata->componentTypes) {
            if (const auto* schema = registry.GetSchema(componentTypeID)) {
                if (const void* componentData = ecsManager.GetComponent(m_EntityID, componentTypeID)) {
                    ComponentSnapshot componentSnapshot(componentTypeID);
                    if (componentSnapshot.CaptureComponent(*schema, componentData, context)) {
                        m_ComponentSnapshots[componentTypeID] = componentSnapshot;
                    }
                }
            }
        }
        m_ComponentCount = m_ComponentSnapshots.size();
        m_HasCapturedData = m_ComponentCount > 0;
        m_IsValid = m_HasCapturedData; // Set IsValid based on whether data was captured
        return m_HasCapturedData;
    }

    uint32_t GetEntityID() const { return m_EntityID; }
    uint32_t GetEntityGeneration() const { return m_EntityGeneration; } // Added getter for generation

    const std::unordered_map<uint32_t, ComponentSnapshot>& GetComponentSnapshots() const {
        return m_ComponentSnapshots;
    }

    std::unordered_map<uint32_t, ComponentSnapshot>& GetComponentSnapshots() {
        return m_ComponentSnapshots;
    }

    const ComponentSnapshot* GetComponentSnapshot(uint32_t componentTypeID) const {
        auto it = m_ComponentSnapshots.find(componentTypeID);
        return it != m_ComponentSnapshots.end() ? &it->second : nullptr;
    }

    bool IsValid() const { return m_IsValid; }
    size_t GetComponentCount() const { return m_ComponentCount; } // Added getter for component count
    size_t GetTotalDataSize() const {
        size_t total = 0;
        for (const auto& pair : m_ComponentSnapshots) {
            total += pair.second.GetTotalDataSize(); // Assuming ComponentSnapshot has GetTotalDataSize
        }
        return total;
    }


    void Serialize(BinaryWriter& writer) const {
        writer.WriteU32(m_EntityID);
        writer.WriteU32(m_EntityGeneration);
        writer.WriteU32(static_cast<uint32_t>(m_ComponentSnapshots.size()));
        for (const auto& pair : m_ComponentSnapshots) {
            pair.second.Serialize(writer);
        }
    }

    void Deserialize(BinaryReader& reader, const ComponentSchemaRegistry& registry) {
        m_EntityID = reader.ReadU32();
        m_EntityGeneration = reader.ReadU32();
        uint32_t componentCount = reader.ReadU32();
        m_ComponentSnapshots.clear(); // Clear existing snapshots before deserializing
        for (uint32_t i = 0; i < componentCount; ++i) {
            ComponentSnapshot componentSnapshot;
            componentSnapshot.Deserialize(reader, registry);
            m_ComponentSnapshots[componentSnapshot.GetComponentTypeID()] = componentSnapshot;
        }
        m_ComponentCount = m_ComponentSnapshots.size();
        m_HasCapturedData = m_ComponentCount > 0;
        m_IsValid = m_HasCapturedData;
    }

    std::string GetDebugSummary() const {
        std::string summary = "EntityID: " + std::to_string(m_EntityID) + ", Generation: " + std::to_string(m_EntityGeneration) + "\n";
        for (const auto& pair : m_ComponentSnapshots) {
            summary += "  " + pair.second.GetDebugSummary() + "\n";
        }
        return summary;
    }
};