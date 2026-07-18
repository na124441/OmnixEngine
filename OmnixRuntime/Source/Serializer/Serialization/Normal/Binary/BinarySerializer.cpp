#include "Serializer/Serialization/Normal/Binary/BinarySerializer.h"
#include <stdexcept>
#include <sstream>

BinarySerializer::BinarySerializer(BinaryWriter& writer)
    : m_writer(writer)
{
}

// 1. SerializeSnapshot(snapshot, writer)
void BinarySerializer::SerializeSnapshot(const SnapshotData& snapshot)
{
    m_writer.BeginDocument();                   // i
    m_writer.BeginScope(SCOPE_SNAPSHOT);        // ii
    SerializeHeader(snapshot.header);           // iii
    m_writer.BeginScope(SCOPE_ENTITIES);        // iv

    // v. For each entity in snapshot.entities:
    for (const EntityData& entity : snapshot.entities)
    {
        SerializeEntity(entity);
    }

    m_writer.EndScope();                        // vi. End Entities
    m_writer.EndScope();                        // vii. End SNAPSHOT
    m_writer.EndDocument();                     // viii
}

// 2. SerializeHeader(header)
void BinarySerializer::SerializeHeader(const SnapshotHeaderData& header)
{
    m_writer.BeginScope(SCOPE_HEADER);          // i

    m_writer.WriteU16(header.version);          // ii

    m_writer.WriteString(header.engineName); // iii & iv

    m_writer.WriteU64(header.timestamp);        // v

    m_writer.EndScope();                        // vi
}

// 3. SerializeEntity(entity)
void BinarySerializer::SerializeEntity(const EntityData& entity)
{
    m_writer.BeginScope(SCOPE_ENTITY);          // i

    m_writer.WriteU64(entity.id);               // ii
    m_writer.WriteU16(entity.GetComponentCount()); // iii

    // iv. For each component in entity.components:
    for (const ComponentData& component : entity.components)
    {
        SerializeComponent(component);
    }

    m_writer.EndScope();                        // v
}

// 4. SerializeComponent(component)
void BinarySerializer::SerializeComponent(const ComponentData& component)
{
    m_writer.BeginScope(SCOPE_COMPONENT);       // i

    m_writer.WriteU32(component.typeID);        // ii
    m_writer.WriteU16(component.version);       // iii
    m_writer.WriteU16(component.GetFieldCount()); // iv

    // v. For each field in component.fields:
    for (const FieldData& field : component.fields)
    {
        SerializeField(field);
    }

    m_writer.EndScope();                        // vi
}

// 5. SerializeField(field)
void BinarySerializer::SerializeField(const FieldData& field)
{
    m_writer.BeginScope(SCOPE_FIELD);           // i

    m_writer.WriteU32(field.id);                // ii
    m_writer.WriteString(field.typeString); // Write type string

    SerializeFieldValue(field);                 // iv - Serialize value based on type

    m_writer.EndScope();                        // v
}

// Helper method to serialize field values based on type
void BinarySerializer::SerializeFieldValue(const FieldData& field)
{
    // iv. Switch(field.type):
    switch (field.type)
    {
    case FieldType::INT:
    {
        // -INT --> writer.I32(value)
        int32_t intValue = std::stoi(field.value);
        m_writer.WriteI32(intValue);
        break;
    }

    case FieldType::FLOAT:
    {
        // -FLOAT --> writer.F32(value)
        float floatValue = std::stof(field.value);
        m_writer.WriteF32(floatValue);
        break;
    }

    case FieldType::DOUBLE:
    {
        double doubleValue = std::stod(field.value);
        m_writer.WriteF64(doubleValue);
        break;
    }

    case FieldType::BOOL:
    {
        bool boolValue = (field.value == "true" || field.value == "1");
        m_writer.WriteBool(boolValue);
        break;
    }

    case FieldType::VEC3:
    {
        // -VEC3 --> writer.F32(x), writer.F32(y), writer.F32(z)
        // Assuming value format: "x,y,z"
        std::istringstream iss(field.value);
        float x, y, z;
        char comma;
        iss >> x >> comma >> y >> comma >> z;
        m_writer.WriteF32(x);
        m_writer.WriteF32(y);
        m_writer.WriteF32(z);
        break;
    }

    case FieldType::VEC4:
    {
        // -VEC4 --> writer.F32(x), writer.F32(y), writer.F32(z), writer.F32(w)
        // Assuming value format: "x,y,z,w"
        std::istringstream iss(field.value);
        float x, y, z, w;
        char comma;
        iss >> x >> comma >> y >> comma >> z >> comma >> w;
        m_writer.WriteF32(x);
        m_writer.WriteF32(y);
        m_writer.WriteF32(z);
        m_writer.WriteF32(w);
        break;
    }

    case FieldType::STRING:
    {
        // -STRING --> writer.U32(value.length()), writer.WriteBytes(value.data(), value.length())
        m_writer.WriteString(field.value);
        break;
    }

    case FieldType::BLOB:
    {
        // -BLOB --> writer.U32(value.size()), writer.WriteBytes(value.data(), value.size())
        // For BLOB, value is expected to be hex-encoded string representation
        std::vector<uint8_t> blobData;
        for (size_t i = 0; i < field.value.length(); i += 2)
        {
            std::string byteString = field.value.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
            blobData.push_back(byte);
        }
        m_writer.WriteBytes(blobData);
        break;
    }

    default:
        throw std::runtime_error("Unknown field type: " + std::to_string(static_cast<int>(field.type)));
    }
}