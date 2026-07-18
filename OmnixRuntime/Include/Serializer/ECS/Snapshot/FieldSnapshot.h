#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <iomanip>
#include "../../Core/Logger.h"
#include "../ComponentSchema.h"

// Forward declarations
class Serializer;
class Deserializer;

class ByteSpan {
private:
    uint8_t* m_Data;
    size_t m_Size;

public:
    ByteSpan() : m_Data(nullptr), m_Size(0) {}
    ByteSpan(void* data, size_t size) : m_Data(static_cast<uint8_t*>(data)), m_Size(size) {}

    uint8_t* Data() { return m_Data; }
    const uint8_t* Data() const { return m_Data; }
    size_t Size() const { return m_Size; }
    bool IsValid() const { return m_Data != nullptr && m_Size > 0; }

    void CopyFrom(const void* source) {
        if (IsValid() && source) { std::memcpy(m_Data, source, m_Size); }
    }
    void CopyTo(void* destination) const {
        if (IsValid() && destination) { std::memcpy(destination, m_Data, m_Size); }
    }
    bool Equals(const ByteSpan& other) const {
        if (m_Size != other.m_Size) return false;
        if (!m_Data || !other.m_Data) return false;
        return std::memcmp(m_Data, other.m_Data, m_Size) == 0;
    }
};

class FieldSnapshot {
private:
    uint32_t m_FieldID;
    FieldType m_FieldType;
    size_t m_FieldSize;
    std::vector<uint8_t> m_FieldDataBuffer;
    ByteSpan m_FieldData;
    std::vector<uint8_t> m_PreviousValue;
    bool m_HasChanged;

public:
    FieldSnapshot();
    FieldSnapshot(uint32_t fieldID, FieldType fieldType, size_t fieldSize);

    // Custom copy constructor
    FieldSnapshot(const FieldSnapshot& other)
        : m_FieldID(other.m_FieldID),
          m_FieldType(other.m_FieldType),
          m_FieldSize(other.m_FieldSize),
          m_FieldDataBuffer(other.m_FieldDataBuffer),
          m_PreviousValue(other.m_PreviousValue),
          m_HasChanged(other.m_HasChanged)
    {
        m_FieldData = ByteSpan(m_FieldDataBuffer.data(), m_FieldSize);
    }

    // Custom copy assignment
    FieldSnapshot& operator=(const FieldSnapshot& other) {
        if (this != &other) {
            m_FieldID = other.m_FieldID;
            m_FieldType = other.m_FieldType;
            m_FieldSize = other.m_FieldSize;
            m_FieldDataBuffer = other.m_FieldDataBuffer;
            m_PreviousValue = other.m_PreviousValue;
            m_HasChanged = other.m_HasChanged;
            m_FieldData = ByteSpan(m_FieldDataBuffer.data(), m_FieldSize);
        }
        return *this;
    }

    // Custom move constructor
    FieldSnapshot(FieldSnapshot&& other) noexcept
        : m_FieldID(other.m_FieldID),
          m_FieldType(other.m_FieldType),
          m_FieldSize(other.m_FieldSize),
          m_FieldDataBuffer(std::move(other.m_FieldDataBuffer)),
          m_PreviousValue(std::move(other.m_PreviousValue)),
          m_HasChanged(other.m_HasChanged)
    {
        m_FieldData = ByteSpan(m_FieldDataBuffer.data(), m_FieldSize);
        other.m_FieldData = ByteSpan();
    }

    // Custom move assignment
    FieldSnapshot& operator=(FieldSnapshot&& other) noexcept {
        if (this != &other) {
            m_FieldID = other.m_FieldID;
            m_FieldType = other.m_FieldType;
            m_FieldSize = other.m_FieldSize;
            m_FieldDataBuffer = std::move(other.m_FieldDataBuffer);
            m_PreviousValue = std::move(other.m_PreviousValue);
            m_HasChanged = other.m_HasChanged;
            m_FieldData = ByteSpan(m_FieldDataBuffer.data(), m_FieldSize);
            other.m_FieldData = ByteSpan();
        }
        return *this;
    }

    static void* ComputeFieldAddress(void* basePtr, size_t offset);
    void CaptureField(void* componentPtr, size_t fieldOffset);
    void ApplyField(void* componentPtr, size_t fieldOffset) const;

    uint32_t GetFieldID() const { return m_FieldID; }
    const ByteSpan& GetFieldData() const { return m_FieldData; }
    bool HasChanged() const { return m_HasChanged; }

    void Serialize(Serializer& writer) const;
    void Deserialize(Deserializer& reader);

    bool IsValid() const;
    bool ValidateAgainstSchema(const FieldSchema& schema) const {
        if (m_FieldType != schema.type) {
            LOG_WARN("Field type mismatch: %u vs %u", static_cast<uint32_t>(m_FieldType), static_cast<uint32_t>(schema.type));
            return false;
        }
        if (m_FieldSize != schema.size) {
            LOG_WARN("Field size mismatch: %zu vs %zu", m_FieldSize, schema.size);
            return false;
        }
        return true;
    }
    std::string GetDebugString() const;
};

// Helper to convert field data to a hex string for comparison/logging
inline std::string FieldDataToString(const FieldSnapshot& field)
{
    const auto& data = field.GetFieldData();
    if (!data.IsValid()) return "";

    std::stringstream ss;
    for (size_t i = 0; i < data.Size(); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data.Data()[i]);
    }
    return ss.str();
}