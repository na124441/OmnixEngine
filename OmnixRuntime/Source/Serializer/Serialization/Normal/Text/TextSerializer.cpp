#include "Serializer/Serialization/Normal/Text/TextSerializer.h"

TextSerializer::TextSerializer(TextWriter& writer)
    : m_writer(writer)
{
}

// 1. SerializeSnapshot(snapshot)
void TextSerializer::SerializeSnapshot(const Snapshot& snapshot)
{
    m_writer.BeginScope("SNAPSHOT");    // ii
    SerializeHeader(snapshot.header);   // iii
    m_writer.BeginScope("Entities");    // iv

    // v. For each entity in snapshot.entities:
    for (const Entity& entity : snapshot.entities)
    {
        SerializeEntity(entity);
    }

    m_writer.EndScope();                // vi. End Entities
    m_writer.EndScope();                // vii. End SNAPSHOT
}

// 2. SerializeHeader(header)
void TextSerializer::SerializeHeader(const SnapshotHeader& header)
{
    m_writer.BeginScope("HEADER");      // i

    m_writer.WriteKey("Version");       // ii
    m_writer.WriteValue(header.version); // iii

    m_writer.WriteKey("Engine");        // v
    m_writer.WriteValue(header.engineName); // vi

    m_writer.WriteKey("Timestamp");     // viii
    m_writer.WriteValue((int)header.timestamp); // ix

    m_writer.EndScope();                // xi
}

// 3. SerializeEntity(entity)
void TextSerializer::SerializeEntity(const Entity& entity)
{
    m_writer.BeginScope("ENTITY");      // i

    m_writer.WriteKey("ID");            // ii
    m_writer.WriteValue((int)entity.id);     // iii

    m_writer.WriteKey("ComponentCount"); // v
    m_writer.WriteValue(entity.GetComponentCount()); // vi

    // viii. For each component in entity.components:
    for (const Component& component : entity.components)
    {
        SerializeComponent(component);
    }

    m_writer.EndScope();                // ix
}

// 4. SerializeComponent(component)
void TextSerializer::SerializeComponent(const Component& component)
{
    m_writer.BeginScope("COMPONENT");   // i

    m_writer.WriteKey("Type");          // ii
    m_writer.WriteValue((int)component.typeID); // iii

    m_writer.WriteKey("Version");       // v
    m_writer.WriteValue(component.version); // vi

    m_writer.WriteKey("Field_Count");   // viii
    m_writer.WriteValue(component.GetFieldCount()); // ix

    // xi. For each field in component.fields:
    for (const Field& field : component.fields)
    {
        SerializeField(field);
    }

    m_writer.EndScope();                // xii
}

// 5. SerializeField(field)
void TextSerializer::SerializeField(const Field& field)
{
    m_writer.BeginScope("FIELD");       // i

    m_writer.WriteKey("ID");            // ii
    m_writer.WriteValue((int)field.id);      // iii

    m_writer.WriteKey("Type");          // v
    m_writer.WriteValue(field.type);    // vi

    m_writer.WriteKey("Value");         // viii
    m_writer.WriteValue(field.value);   // ix

    m_writer.EndScope();                // xi
}