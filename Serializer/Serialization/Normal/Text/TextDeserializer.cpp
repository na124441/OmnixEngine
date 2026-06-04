#include "TextDeserializer.h"
#include <iostream>

TextDeserializer::TextDeserializer(TextReader& reader)
    : m_reader(reader)
{
}

void TextDeserializer::LogWarning(const std::string& message)
{
    std::cerr << "TextDeserializer Warning: " << message << std::endl;
}

// 1. DeserializeSnapshot(reader)
Snapshot TextDeserializer::DeserializeSnapshot()
{
    m_reader.BeginDocument();               // i
    m_reader.BeginScope("SNAPSHOT");        // ii

    SnapshotHeader header = DeserializeHeader(); // iii

    m_reader.BeginScope("Entities");        // iv
    std::vector<Entity> entities;           // v

    // This is tricky without a PeekToken. We'll assume the file is well-formed.
    // A better approach would be for the reader to signal the end of a scope.
    try {
        while (true) {
            entities.push_back(DeserializeEntity());
        }
    } catch (const std::runtime_error&) {
        // This is a bit of a hack to detect the end of the entities.
        // A better reader API would avoid this.
    }


    m_reader.EndScope();                    // vii. End Entities
    m_reader.EndScope();                    // viii. End SNAPSHOT
    m_reader.EndDocument();

    Snapshot snapshot;                      // ix. return Snapshot(header, entities)
    snapshot.header = header;
    snapshot.entities = entities;
    return snapshot;
}

// 2. DeserializeHeader()
SnapshotHeader TextDeserializer::DeserializeHeader()
{
    m_reader.BeginScope("HEADER");          // i

    m_reader.ReadKey("Version");            // ii
    int version = m_reader.ReadValue_Int(); // iii

    m_reader.ReadKey("Engine");             // iv
    std::string engineName = m_reader.ReadValue_String(); // v

    m_reader.ReadKey("Timestamp");          // vi
    long long timestamp = m_reader.ReadValue_Int(); // vii (Note: requires ReadValue_Long() in TextReader)

    m_reader.EndScope();                    // viii

    SnapshotHeader header;                  // ix. return SnapshotHeader(...)
    header.version = version;
    header.engineName = engineName;
    header.timestamp = timestamp;
    return header;
}

// 3. DeserializeEntity()
Entity TextDeserializer::DeserializeEntity()
{
    m_reader.BeginScope("ENTITY");          // i

    m_reader.ReadKey("ID");                 // ii
    int id = m_reader.ReadValue_Int();      // iii

    m_reader.ReadKey("ComponentCount");     // iv
    int componentCount = m_reader.ReadValue_Int(); // v

    std::vector<Component> components;      // vi

    for (int i = 0; i < componentCount; ++i) {
        components.push_back(DeserializeComponent());
    }

    // viii. If componentCount != components.size() --> Warning
    if (componentCount != static_cast<int>(components.size()))
    {
        LogWarning("Entity component count mismatch: expected " + std::to_string(componentCount) +
            ", got " + std::to_string(components.size()));
    }

    m_reader.EndScope();                    // ix

    Entity entity;                          // x. return Entity(id, components)
    entity.id = id;
    entity.components = components;
    return entity;
}

// 4. DeserializeComponent()
Component TextDeserializer::DeserializeComponent()
{
    m_reader.BeginScope("COMPONENT");       // i

    m_reader.ReadKey("Type");               // ii
    int typeID = m_reader.ReadValue_Int();  // iii

    m_reader.ReadKey("Version");            // iv
    int version = m_reader.ReadValue_Int(); // v

    m_reader.ReadKey("Field_Count");        // vi
    int fieldCount = m_reader.ReadValue_Int(); // vii

    std::vector<Field> fields;              // viii

    for (int i = 0; i < fieldCount; ++i) {
        fields.push_back(DeserializeField());
    }


    // xi. If fieldCount != fields.size() --> Warning
    if (fieldCount != static_cast<int>(fields.size()))
    {
        LogWarning("Component field count mismatch: expected " + std::to_string(fieldCount) +
            ", got " + std::to_string(fields.size()));
    }

    m_reader.EndScope();                    // xii

    Component component;                    // xiii. return Component(typeID, version, fields)
    component.typeID = typeID;
    component.version = version;
    component.fields = fields;
    return component;
}

// 5. DeserializeField()
Field TextDeserializer::DeserializeField()
{
    m_reader.BeginScope("FIELD");           // i

    m_reader.ReadKey("ID");                 // ii
    int id = m_reader.ReadValue_Int();      // iii

    m_reader.ReadKey("Type");               // iv
    std::string type = m_reader.ReadValue_String(); // v

    m_reader.ReadKey("Value");              // vi
    std::string value = m_reader.ReadValue_String(); // vii

    m_reader.EndScope();                    // viii

    Field field(id, type, value);           // ix. return Field(id, type, value)
    return field;
}