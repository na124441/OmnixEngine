#include "Core/Serialization/Normal/NormalDeserializer.h"
#include "Serializer/ECS/Snapshot/ECSSnapshot.h"
#include "Serializer/ECS/Snapshot/EntitySnapshot.h"
#include "Serializer/ECS/Snapshot/ComponentSnapshot.h"
#include "Serializer/ECS/Snapshot/FieldSnapshot.h"
#include "Serializer/ECS/ECS.h"
#include "Core/Logger.h"
#include <cstring>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

NormalDeserializer::NormalDeserializer(const SerializerContext* context)
    : m_Data(nullptr), m_Size(0), m_Position(0), m_CurrentSnapshot(nullptr),
      m_OwnsSnapshot(false), m_IsValid(false), m_EntitiesDeserialized(0),
      m_ComponentsDeserialized(0), m_FieldsDeserialized(0)
{
    // Context is currently unused but kept for future compatibility.
}

NormalDeserializer::~NormalDeserializer()
{
    if (m_OwnsSnapshot && m_CurrentSnapshot) {
        delete m_CurrentSnapshot;
    }
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

bool NormalDeserializer::Deserialize(const uint8_t* data, size_t size, ECS& ecsManager, const ComponentSchemaRegistry& registry)
{
    m_CurrentSnapshot = DeserializeToSnapshot(data, size, registry);
    if (!m_CurrentSnapshot) {
        return false;
    }
    m_OwnsSnapshot = true;
    // The caller is responsible for applying the snapshot to the ECS.
    // This separation makes the deserializer's role clearer.
    return true;
}

ECSSnapshot* NormalDeserializer::DeserializeToSnapshot(const uint8_t* data, size_t size, const ComponentSchemaRegistry& registry)
{
    m_Data = data;
    m_Size = size;
    m_Position = 0;
    m_IsValid = false;
    m_LastError = "";
    m_EntitiesDeserialized = 0;
    m_ComponentsDeserialized = 0;
    m_FieldsDeserialized = 0;

    if (!InitializeSnapshot()) {
        m_LastError = "Failed to initialize snapshot.";
        return nullptr;
    }

    uint32_t entityCount;
    if (!ReadEntityCount(entityCount)) {
        m_LastError = "Failed to read entity count.";
        return nullptr;
    }

    m_CurrentSnapshot->GetEntitySnapshots().reserve(entityCount);

    for (uint32_t i = 0; i < entityCount; ++i) {
        uint32_t entityID, generation, componentCount;
        if (!BeginEntity(entityID, generation, componentCount)) {
            m_LastError = "Failed to read entity info.";
            return nullptr;
        }

        // Construct the EntitySnapshot in-place to avoid copy/move issues.
        m_CurrentSnapshot->GetEntitySnapshots().emplace_back(entityID, generation);
        EntitySnapshot& entitySnapshot = m_CurrentSnapshot->GetEntitySnapshots().back();

        for (uint32_t j = 0; j < componentCount; ++j) {
            uint32_t componentTypeID, componentVersion, fieldCount;
            if (!BeginComponent(componentTypeID, componentVersion, fieldCount)) {
                m_LastError = "Failed to read component info.";
                return nullptr;
            }

            auto& componentSnapshot = entitySnapshot.GetComponentSnapshots().emplace(componentTypeID, componentTypeID).first->second;

            for (uint32_t k = 0; k < fieldCount; ++k) {
                uint32_t fieldID;
                uint8_t fieldType;
                uint32_t fieldSize;
                std::vector<uint8_t> fieldData;

                if (!ReadField(fieldID, fieldType, fieldSize, fieldData)) {
                    m_LastError = "Failed to read field info/data.";
                    return nullptr;
                }

                // We capture the read buffer into the field snapshot
                FieldSnapshot fieldSnapshot(fieldID, static_cast<FieldType>(fieldType), fieldSize);
                fieldSnapshot.CaptureField(fieldData.data(), 0);
                componentSnapshot.GetFieldSnapshots().push_back(fieldSnapshot);

                m_FieldsDeserialized++;
            }
            m_ComponentsDeserialized++;
        }
        m_EntitiesDeserialized++;
    }

    if (!FinalizeSnapshot()) return nullptr;

    m_IsValid = true;
    ECSSnapshot* result = m_CurrentSnapshot;
    m_CurrentSnapshot = nullptr; // Transfer ownership to the caller.
    return result;
}

// ============================================================================
// LOW-LEVEL READING HELPERS
// ============================================================================

bool NormalDeserializer::CanRead(size_t bytes) const
{
    return (m_Position + bytes) <= m_Size;
}

void NormalDeserializer::Advance(size_t bytes)
{
    m_Position += bytes;
}

bool NormalDeserializer::ReadUInt8(uint8_t& value)
{
    if (!CanRead(1)) return false;
    value = m_Data[m_Position++];
    return true;
}

bool NormalDeserializer::ReadUInt32(uint32_t& value)
{
    if (!CanRead(4)) return false;
    std::memcpy(&value, &m_Data[m_Position], 4);
    m_Position += 4;
    return true;
}

bool NormalDeserializer::ReadUInt64(uint64_t& value)
{
    if (!CanRead(8)) return false;
    std::memcpy(&value, &m_Data[m_Position], 8);
    m_Position += 8;
    return true;
}

bool NormalDeserializer::ReadFloat(float& value)
{
    return ReadUInt32(reinterpret_cast<uint32_t&>(value));
}

bool NormalDeserializer::ReadBytes(uint8_t* buffer, size_t size)
{
    if (!CanRead(size)) return false;
    std::memcpy(buffer, &m_Data[m_Position], size);
    m_Position += size;
    return true;
}

bool NormalDeserializer::ReadString(std::string& str)
{
    uint32_t len;
    if (!ReadUInt32(len)) return false;
    if (!CanRead(len)) return false;
    str.assign(reinterpret_cast<const char*>(&m_Data[m_Position]), len);
    m_Position += len;
    return true;
}

// ============================================================================
// DESERIALIZATION PIPELINE
// ============================================================================

bool NormalDeserializer::InitializeSnapshot()
{
    m_CurrentSnapshot = new ECSSnapshot();
    return true;
}

bool NormalDeserializer::ReadEntityCount(uint32_t& outCount)
{
    return ReadUInt32(outCount);
}

bool NormalDeserializer::BeginEntity(uint32_t& outEntityID, uint32_t& outGeneration, uint32_t& outComponentCount)
{
    outGeneration = 0; // Not written by NormalSerializer
    return ReadUInt32(outEntityID) && ReadUInt32(outComponentCount);
}

bool NormalDeserializer::BeginComponent(uint32_t& outComponentTypeID, uint32_t& outComponentVersion, uint32_t& outFieldCount)
{
    outComponentVersion = 0; // Not written by NormalSerializer
    return ReadUInt32(outComponentTypeID) && ReadUInt32(outFieldCount);
}

bool NormalDeserializer::ReadField(uint32_t& outFieldID, uint8_t& outFieldType, uint32_t& outFieldSize, std::vector<uint8_t>& outFieldData)
{
    outFieldType = 0; // Not written by NormalSerializer
    if (!ReadUInt32(outFieldID) || !ReadUInt32(outFieldSize)) return false;
    outFieldData.resize(outFieldSize);
    return ReadBytes(outFieldData.data(), outFieldSize);
}

bool NormalDeserializer::FinalizeSnapshot()
{
    // Placeholder for future validation, like checking a checksum.
    return true;
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

std::string NormalDeserializer::GetLastError() const { return m_LastError; }
size_t NormalDeserializer::GetBytesRead() const { return m_Position; }
bool NormalDeserializer::WasSuccessful() const { return m_IsValid; }
std::string NormalDeserializer::GetDeserializerName() const { return "NormalDeserializer"; }
uint32_t NormalDeserializer::GetFormatVersion() const { return 1; }
uint32_t NormalDeserializer::GetEntitiesDeserialized() const { return m_EntitiesDeserialized; }
uint32_t NormalDeserializer::GetComponentsDeserialized() const { return m_ComponentsDeserialized; }
uint32_t NormalDeserializer::GetFieldsDeserialized() const { return m_FieldsDeserialized; }
