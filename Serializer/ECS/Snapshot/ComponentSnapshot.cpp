#include "ComponentSnapshot.h"
#include "FieldSnapshot.h"
#include "../../Serialization/Normal/Binary/BinaryWriter.h"
#include "../../Serialization/Normal/Binary/BinaryReader.h"
#include "../../Core/Logger.h"

ComponentSnapshot::ComponentSnapshot()
    : m_ComponentTypeID(0), m_Version(0), m_Schema(nullptr), m_Context(nullptr), m_IsValid(false), m_HasCapturedData(false), m_TotalDataSize(0) {}

ComponentSnapshot::ComponentSnapshot(uint32_t componentTypeID)
    : m_ComponentTypeID(componentTypeID), m_Version(0), m_Schema(nullptr), m_Context(nullptr), m_IsValid(false), m_HasCapturedData(false), m_TotalDataSize(0) {}

bool ComponentSnapshot::CaptureComponent(const ComponentSchema& schema, const void* componentData, const SnapshotContext& context) {
    m_Schema = &schema;
    m_Context = &context;
    m_FieldSnapshots.clear();
    m_TotalDataSize = 0;

    if (!componentData || schema.fieldCount == 0) {
        m_IsValid = false;
        m_HasCapturedData = false;
        return false;
    }

    for (uint32_t i = 0; i < schema.fieldCount; ++i) {
        const FieldSchema& fieldSchema = schema.fields[i];

        // Optional: filter by intent if context requires it
        // if (context.GetType() == SnapshotType::SNAPSHOT_NETWORK && (fieldSchema.intent & INTENT_NETWORK) == 0) continue;

        FieldSnapshot fieldSnapshot(i, fieldSchema.type, fieldSchema.size);
        fieldSnapshot.CaptureField(const_cast<void*>(componentData), fieldSchema.offset);

        m_FieldSnapshots.push_back(fieldSnapshot);
        m_TotalDataSize += fieldSchema.size;
    }

    m_HasCapturedData = !m_FieldSnapshots.empty();
    m_IsValid = m_HasCapturedData;
    return m_HasCapturedData;
}

uint32_t ComponentSnapshot::GetComponentTypeID() const {
    return m_ComponentTypeID;
}

const std::vector<FieldSnapshot>& ComponentSnapshot::GetFieldSnapshots() const {
    return m_FieldSnapshots;
}

std::vector<FieldSnapshot>& ComponentSnapshot::GetFieldSnapshots() {
    return m_FieldSnapshots;
}

bool ComponentSnapshot::IsValid() const {
    return m_IsValid;
}

void ComponentSnapshot::Serialize(BinaryWriter& writer) const {
    writer.WriteU32(m_ComponentTypeID);
    writer.WriteU32(m_Version);
    writer.WriteU32(static_cast<uint32_t>(m_FieldSnapshots.size()));

    for (const auto& fieldSnapshot : m_FieldSnapshots) {
        // We'll write out field ID so it can be deserialized properly
        writer.WriteU32(fieldSnapshot.GetFieldID());
        // Since FieldSnapshot has no standard Serialize/Deserialize with BinaryWriter, we directly serialize its data
        writer.WriteU32(static_cast<uint32_t>(fieldSnapshot.GetFieldData().Size()));

        // Fix: Use public API overloads of WriteBytes
        // writer.WriteBytes(fieldSnapshot.GetFieldData().Data(), fieldSnapshot.GetFieldData().Size());
        const uint8_t* ptr = fieldSnapshot.GetFieldData().Data();
        size_t sz = fieldSnapshot.GetFieldData().Size();
        std::vector<uint8_t> tmp(ptr, ptr + sz);
        writer.WriteBytes(tmp);
    }
}

void ComponentSnapshot::Deserialize(BinaryReader& reader, const ComponentSchemaRegistry& registry) {
    m_ComponentTypeID = reader.ReadU32();
    m_Version = reader.ReadU32();
    uint32_t fieldCount = reader.ReadU32();

    const ComponentSchema* schema = registry.GetSchema(m_ComponentTypeID);

    m_FieldSnapshots.clear();
    m_TotalDataSize = 0;

    for (uint32_t i = 0; i < fieldCount; ++i) {
        uint32_t fieldID = reader.ReadU32();
        uint32_t fieldSize = reader.ReadU32();

        FieldType fType = FieldType::UNKNOWN;
        if (schema && fieldID < schema->fieldCount) {
            fType = schema->fields[fieldID].type;
        }

        FieldSnapshot fieldSnapshot(fieldID, fType, fieldSize);

        // Fix: Use public API overloads of ReadBytes
        std::vector<uint8_t> buffer = reader.ReadBytes(fieldSize);

        // This simulates applying data since FieldSnapshot expects captured data to live in memory
        // We capture it from the buffer.
        fieldSnapshot.CaptureField(buffer.data(), 0);

        m_FieldSnapshots.push_back(fieldSnapshot);
        m_TotalDataSize += fieldSize;
    }

    m_HasCapturedData = !m_FieldSnapshots.empty();
    m_IsValid = m_HasCapturedData;
}

std::string ComponentSnapshot::GetDebugSummary() const {
    std::string summary = "ComponentTypeID: " + std::to_string(m_ComponentTypeID) + " (Fields: " + std::to_string(m_FieldSnapshots.size()) + ")";
    return summary;
}