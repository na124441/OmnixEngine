#include "BinaryDeserializer.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

BinaryDeserializer::BinaryDeserializer(BinaryReader& reader)
    : m_reader(reader)
{
}

// 1. DeserializeSnapshot(reader)
SnapshotData BinaryDeserializer::DeserializeSnapshot()
{
    m_reader.BeginDocument();                   // i
    m_reader.BeginScope(SCOPE_SNAPSHOT);        // ii

    SnapshotHeaderData header = DeserializeHeader(); // iii

    m_reader.BeginScope(SCOPE_ENTITIES);        // iv
    std::vector<EntityData> entities;           // v

    // vi. while reader.HasMoreScopes():
    // In binary format, we rely on scope boundaries, so we check if we can read more entities
    try
    {
        while (m_reader.PeekScopeID() == SCOPE_ENTITY)
        {
            EntityData entity = DeserializeEntity(); // vii
            entities.push_back(entity);             // viii
        }
    }
    catch (const std::exception&)
    {
        // End of scope reached, this is expected
    }

    m_reader.EndScope();                        // ix. End Entities
    m_reader.EndScope();                        // x. End SNAPSHOT
    m_reader.EndDocument();

    SnapshotData snapshot;                      // xi. return Snapshot(header, entities)
    snapshot.header = header;
    snapshot.entities = entities;
    return snapshot;
}

// 2. DeserializeHeader()
SnapshotHeaderData BinaryDeserializer::DeserializeHeader()
{
    m_reader.BeginScope(SCOPE_HEADER);          // i

    uint16_t version = m_reader.ReadU16();      // ii

    std::string engineName = m_reader.ReadString(); // iii

    uint64_t timestamp = m_reader.ReadU64();    // iv

    m_reader.EndScope();                        // v

    SnapshotHeaderData header;                  // vi. Return Header(version, engineName, timestamp)
    header.version = version;
    header.engineName = engineName;
    header.timestamp = timestamp;
    return header;
}

// 3. DeserializeEntity(reader)
EntityData BinaryDeserializer::DeserializeEntity()
{
    m_reader.BeginScope(SCOPE_ENTITY);          // i

    uint64_t id = m_reader.ReadU64();           // ii
    uint16_t expectedCount = m_reader.ReadU16(); // iii

    std::vector<ComponentData> components;      // iv

    // v. Repeat expectedCount times:
    for (uint16_t i = 0; i < expectedCount; ++i)
    {
        ComponentData component = DeserializeComponent(); // Read component
        components.push_back(component);
    }

    m_reader.EndScope();                        // vi

    EntityData entity;                          // vii. Return EntityData(id, components)
    entity.id = id;
    entity.components = components;
    return entity;
}

// 4. DeserializeComponent(reader)
ComponentData BinaryDeserializer::DeserializeComponent()
{
    m_reader.BeginScope(SCOPE_COMPONENT);       // i

    uint32_t typeID = m_reader.ReadU32();       // ii
    uint16_t version = m_reader.ReadU16();      // iii
    uint16_t expectedCount = m_reader.ReadU16(); // iv

    std::vector<FieldData> fields;              // v

    // vi. Repeat expectedCount times:
    for (uint16_t i = 0; i < expectedCount; ++i)
    {
        FieldData field = DeserializeField();   // Read field
        fields.push_back(field);
    }

    m_reader.EndScope();                        // vii

    ComponentData component;                    // viii. Return ComponentData(typeID, version, fields)
    component.typeID = typeID;
    component.version = version;
    component.fields = fields;
    return component;
}

// 5. DeserializeField(reader)
FieldData BinaryDeserializer::DeserializeField()
{
    m_reader.BeginScope(SCOPE_FIELD);           // i

    uint32_t fieldID = m_reader.ReadU32();      // ii
    std::string typeString = m_reader.ReadString(); // iii

    // Convert type string to enum
    FieldType fieldType = FieldType::STRING;  // Default
    if (typeString == "INT") fieldType = FieldType::INT;
    else if (typeString == "FLOAT") fieldType = FieldType::FLOAT;
    else if (typeString == "VEC3") fieldType = FieldType::VEC3;
    else if (typeString == "VEC4") fieldType = FieldType::VEC4;
    else if (typeString == "STRING") fieldType = FieldType::STRING;
    else if (typeString == "BLOB") fieldType = FieldType::BLOB;
    else if (typeString == "BOOL") fieldType = FieldType::BOOL;
    else if (typeString == "DOUBLE") fieldType = FieldType::DOUBLE;

    FieldData field;                            // Create field object
    field.id = fieldID;
    field.type = fieldType;
    field.typeString = typeString;

    // iv. Switch(fieldType) - Deserialize value based on type
    DeserializeFieldValue(field, fieldType);

    m_reader.EndScope();                        // v

    // vi. Return FieldData(fieldID, fieldType, value)
    return field;
}

// Helper method to deserialize field values based on type
void BinaryDeserializer::DeserializeFieldValue(FieldData& field, FieldType fieldType)
{
    // iv. Switch(fieldType):
    switch (fieldType)
    {
    case FieldType::INT:
    {
        // -INT --> value = reader.ReadI32()
        int32_t intValue = m_reader.ReadI32();
        field.value = std::to_string(intValue);
        break;
    }

    case FieldType::FLOAT:
    {
        // -FLOAT --> value = reader.ReadF32()
        float floatValue = m_reader.ReadF32();
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << floatValue;
        field.value = oss.str();
        break;
    }

    case FieldType::DOUBLE:
    {
        double doubleValue = m_reader.ReadF64();
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << doubleValue;
        field.value = oss.str();
        break;
    }

    case FieldType::BOOL:
    {
        bool boolValue = m_reader.ReadBool();
        field.value = boolValue ? "true" : "false";
        break;
    }

    case FieldType::VEC3:
    {
        // -VEC3 --> ReadF32() x 3
        float x = m_reader.ReadF32();
        float y = m_reader.ReadF32();
        float z = m_reader.ReadF32();

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << x << ","
            << std::fixed << std::setprecision(6) << y << ","
            << std::fixed << std::setprecision(6) << z;
        field.value = oss.str();
        break;
    }

    case FieldType::VEC4:
    {
        // -VEC4 --> ReadF32 x 4
        float x = m_reader.ReadF32();
        float y = m_reader.ReadF32();
        float z = m_reader.ReadF32();
        float w = m_reader.ReadF32();

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << x << ","
            << std::fixed << std::setprecision(6) << y << ","
            << std::fixed << std::setprecision(6) << z << ","
            << std::fixed << std::setprecision(6) << w;
        field.value = oss.str();
        break;
    }

    case FieldType::STRING:
    {
        // -STRING --> reader.ReadString()
        field.value = m_reader.ReadString();
        break;
    }

    case FieldType::BLOB:
    {
        // -BLOB --> reader.ReadU32() + data = ReadBytes(size)
        std::vector<uint8_t> blobData = m_reader.ReadBytes(m_reader.ReadU32());

        // Convert blob to hex-encoded string
        std::ostringstream oss;
        for (uint8_t byte : blobData)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        field.value = oss.str();
        break;
    }

    default:
        throw std::runtime_error("Unknown field type enum: " + std::to_string(static_cast<int>(fieldType)));
    }
}