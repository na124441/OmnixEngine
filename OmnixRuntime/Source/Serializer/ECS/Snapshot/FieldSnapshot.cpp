#include "Serializer/ECS/Snapshot/FieldSnapshot.h"
#include "Serializer/Serialization/ISerializer.h"
#include "Serializer/Serialization/IDeserializer.h"

FieldSnapshot::FieldSnapshot()
    : m_FieldID(0), m_FieldType(FieldType::UNKNOWN), m_FieldSize(0), m_HasChanged(false) {}

FieldSnapshot::FieldSnapshot(uint32_t fieldID, FieldType fieldType, size_t fieldSize)
    : m_FieldID(fieldID), m_FieldType(fieldType), m_FieldSize(fieldSize), m_HasChanged(false) {}

void* FieldSnapshot::ComputeFieldAddress(void* basePtr, size_t offset) {
    if (!basePtr) return nullptr;
    return static_cast<uint8_t*>(basePtr) + offset;
}

void FieldSnapshot::CaptureField(void* componentPtr, size_t fieldOffset) {
    if (!componentPtr || m_FieldSize == 0) return;

    void* fieldAddr = ComputeFieldAddress(componentPtr, fieldOffset);

    // Save previous value if we already had one
    if (m_FieldData.IsValid()) {
        m_PreviousValue.resize(m_FieldSize);
        std::memcpy(m_PreviousValue.data(), m_FieldData.Data(), m_FieldSize);
    }

    m_FieldDataBuffer.resize(m_FieldSize);
    std::memcpy(m_FieldDataBuffer.data(), fieldAddr, m_FieldSize);

    m_FieldData = ByteSpan(m_FieldDataBuffer.data(), m_FieldSize);

    // Determine if changed
    if (!m_PreviousValue.empty()) {
        m_HasChanged = std::memcmp(m_PreviousValue.data(), m_FieldDataBuffer.data(), m_FieldSize) != 0;
    } else {
        m_HasChanged = true; // First capture
    }
}

void FieldSnapshot::ApplyField(void* componentPtr, size_t fieldOffset) const {
    if (!componentPtr || !m_FieldData.IsValid()) return;

    void* fieldAddr = ComputeFieldAddress(componentPtr, fieldOffset);
    m_FieldData.CopyTo(fieldAddr);
}

void FieldSnapshot::Serialize(Serializer& writer) const {
    // Note: Depending on your exact ISerializer interface, this might differ
    // Usually a higher level serializer writes the field ID and Size, and here we just dump the bytes
    // Or we could have `writer.WriteBytes(m_FieldData.Data(), m_FieldData.Size());`
}

void FieldSnapshot::Deserialize(Deserializer& reader) {
    // Assuming field ID and size are read elsewhere
    m_FieldDataBuffer.resize(m_FieldSize);
    // reader.ReadBytes(m_FieldDataBuffer.data(), m_FieldSize);
    m_FieldData = ByteSpan(m_FieldDataBuffer.data(), m_FieldSize);
    m_HasChanged = true;
}

bool FieldSnapshot::IsValid() const {
    return m_FieldData.IsValid() && m_FieldSize > 0;
}

std::string FieldSnapshot::GetDebugString() const {
    std::stringstream ss;
    ss << "Field[" << m_FieldID << "] Type: " << static_cast<uint32_t>(m_FieldType)
       << " Size: " << m_FieldSize << " Changed: " << (m_HasChanged ? "Yes" : "No");
    return ss.str();
}