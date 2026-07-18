#include "Serializer/Serialization/Normal/NormalSerializer.h"
#include "Serializer/ECS/Snapshot/ECSSnapshot.h"
#include "Serializer/ECS/Snapshot/EntitySnapshot.h"
#include "Serializer/ECS/Snapshot/ComponentSnapshot.h"
#include "Serializer/ECS/Snapshot/FieldSnapshot.h"
#include "../../Core/Logger.h"
#include <cstring>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

NormalSerializer::NormalSerializer()
    : m_Snapshot(nullptr), m_IsValid(false), m_BytesWritten(0)
{
}

NormalSerializer::~NormalSerializer()
{
    CloseFile();
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool NormalSerializer::Serialize(const ECSSnapshot& snapshot)
{
    m_Snapshot = &snapshot;
    m_IsValid = false;
    m_BytesWritten = 0;
    m_LastError = "";

    if (!WriteEntityCount(snapshot.GetEntitySnapshots().size())) return false;

    for (const auto& entity : snapshot.GetEntitySnapshots()) {
        if (!WriteEntity(entity)) return false;
    }

    m_IsValid = true;
    return true;
}

bool NormalSerializer::SerializeToFile(const ECSSnapshot& snapshot, const std::string& filepath)
{
    if (!OpenFile(filepath)) {
        return false;
    }
    bool result = Serialize(snapshot);
    CloseFile();
    return result;
}

bool NormalSerializer::SerializeToBuffer(const ECSSnapshot& snapshot, std::vector<uint8_t>& outBuffer)
{
    m_BufferStream.clear();
    bool result = Serialize(snapshot);
    if (result) {
        outBuffer = m_BufferStream;
    }
    return result;
}

// ============================================================================
// PRIVATE HELPERS - WRITING
// ============================================================================

bool NormalSerializer::WriteUInt32(uint32_t value)
{
    return WriteBytes(&value, sizeof(value));
}

bool NormalSerializer::WriteUInt64(uint64_t value)
{
    return WriteBytes(&value, sizeof(value));
}

bool NormalSerializer::WriteFloat(float value)
{
    return WriteBytes(&value, sizeof(value));
}

bool NormalSerializer::WriteBool(bool value)
{
    return WriteBytes(&value, sizeof(value));
}

bool NormalSerializer::WriteBytes(const void* data, size_t size)
{
    if (m_FileStream.is_open()) {
        m_FileStream.write(static_cast<const char*>(data), size);
        if (m_FileStream.fail()) {
            m_LastError = "Failed to write to file stream.";
            return false;
        }
    } else {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        m_BufferStream.insert(m_BufferStream.end(), bytes, bytes + size);
    }
    m_BytesWritten += size;
    return true;
}

bool NormalSerializer::WriteString(const std::string& str)
{
    if (!WriteUInt32(static_cast<uint32_t>(str.length()))) return false;
    return WriteBytes(str.data(), str.length());
}

bool NormalSerializer::WriteEntityCount(size_t count)
{
    return WriteUInt32(static_cast<uint32_t>(count));
}

bool NormalSerializer::WriteEntity(const EntitySnapshot& entity)
{
    if (!WriteUInt32(entity.GetEntityID())) return false;
    if (!WriteUInt32(static_cast<uint32_t>(entity.GetComponentSnapshots().size()))) return false;

    for (const auto& pair : entity.GetComponentSnapshots()) {
        if (!WriteComponent(pair.second)) return false;
    }
    return true;
}

bool NormalSerializer::WriteComponent(const ComponentSnapshot& component)
{
    if (!WriteUInt32(component.GetComponentTypeID())) return false;
    if (!WriteUInt32(static_cast<uint32_t>(component.GetFieldSnapshots().size()))) return false;

    for (const auto& field : component.GetFieldSnapshots()) {
        if (!WriteField(field)) return false;
    }
    return true;
}

bool NormalSerializer::WriteField(const FieldSnapshot& field)
{
    if (!WriteUInt32(field.GetFieldID())) return false;
    const auto& data = field.GetFieldData();
    if (!WriteUInt32(static_cast<uint32_t>(data.Size()))) return false;
    return WriteBytes(data.Data(), data.Size());
}

// ============================================================================
// PRIVATE HELPERS - FILE I/O
// ============================================================================

bool NormalSerializer::OpenFile(const std::string& filepath)
{
    m_FileStream.open(filepath, std::ios::binary);
    if (!m_FileStream.is_open()) {
        m_LastError = "Failed to open file: " + filepath;
        return false;
    }
    return true;
}

bool NormalSerializer::CloseFile()
{
    if (m_FileStream.is_open()) {
        m_FileStream.close();
    }
    return true;
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

std::string NormalSerializer::GetLastError() const { return m_LastError; }
size_t NormalSerializer::GetBytesWritten() const { return m_BytesWritten; }
bool NormalSerializer::WasSuccessful() const { return m_IsValid; }