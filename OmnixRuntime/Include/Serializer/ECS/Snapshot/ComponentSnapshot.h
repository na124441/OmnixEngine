#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "Serializer/ECS/Snapshot/FieldSnapshot.h"
#include "../SchemaRegistry.h" // For ComponentSchema
#include "Serializer/ECS/Snapshot/SnapshotContext.h"    // For SnapshotContext
#include "../../Serialization/Normal/Binary/BinaryWriter.h" // Use BinaryWriter
#include "../../Serialization/Normal/Binary/BinaryReader.h" // Use BinaryReader

class ComponentSnapshot {
private:
    uint32_t m_ComponentTypeID;
    uint32_t m_Version;
    std::vector<FieldSnapshot> m_FieldSnapshots;
    const ComponentSchema* m_Schema = nullptr; // Initialize
    const SnapshotContext* m_Context = nullptr; // Initialize
    bool m_IsValid = false; // Initialize
    bool m_HasCapturedData = false; // Initialize
    size_t m_TotalDataSize = 0; // Initialize

public:
    ComponentSnapshot();
    explicit ComponentSnapshot(uint32_t componentTypeID);

    bool CaptureComponent(const ComponentSchema& schema, const void* componentData, const SnapshotContext& context);

    uint32_t GetComponentTypeID() const;
    const std::vector<FieldSnapshot>& GetFieldSnapshots() const;
    std::vector<FieldSnapshot>& GetFieldSnapshots();
    bool IsValid() const;
    size_t GetTotalDataSize() const { return m_TotalDataSize; } // Added GetTotalDataSize

    void Serialize(BinaryWriter& writer) const; // Changed to BinaryWriter
    void Deserialize(BinaryReader& reader, const ComponentSchemaRegistry& registry); // Changed to BinaryReader
    std::string GetDebugSummary() const;
};