//1.DeserializerSnapshot(reader)
//  i.reader.BeginDocument();
//  ii.reader.BeginScope("SNAPSHOT");
//  iii.header = DeserializeHeader(reader);
//  iv.reader.BeginScope("Entities");
//  v.entities = empty list
//  vi.while reader.PeekToken == "ENTITY":
//  //      entity = DeserializeEntity(reader);
//  vii.reader.EndScope(); // End Entities
//  viii.reader.EndScope(); // End SNAPSHOT
//  ix.return Snapshot(header, entities);
// 
// 2.DeserializeHeader()
//  i.reader.BeginScope("HEADER");
//  ii.reader.ReadKey("Version");
//  iii.version = reader.ReadValue(Int);
//  iv.reader.ReadKey("Engine");
//  v.engineName = reader.ReadValue(String);
//  vi.reader.ReadKey("Timestamp");
//  vii.timestamp = reader.ReadValue(Long);
//  viii.reader.EndScope();
//  ix.return SnapshotHeader(version, engineName, timestamp);
// 
// 3.DeserializeEntity()
//  i.reader.BeginScope("ENTITY");
//  ii.reader.ReadKey("ID");
//  iii.id = reader.ReadValue(Int);
//  iv.reader.ReadKey("ComponentCount");
//  v.componentCount = reader.ReadValue(Int);
//  vi.components = empty list
//  vii. while reader.PeekToken == "COMPONENT":
//         component = DeserializeComponent(reader);
//  viii.If compoententCount != components.size() --> Warning
//  ix.reader.EndScope();
//  x.return Entity(id, components);
// 
// 4.DeserializeComponent()
//  i.reader.BeginScope("COMPONENT");
//  ii.reader.ReadKey("Type");
//  iii.typeID = reader.ReadValue(Int);
//  iv.reader.ReadKey("Version");
//  v.version = reader.ReadValue(Int);
//  vi.reader.ReadKey("Field_Count");
//  vii.fieldCount = reader.ReadValue(Int);
//  viii.fields = empty list
//  ix. while reader.PeekToken == "FIELD":
//  x.	   field = DeserializeField(reader);
//  xi. If fieldCount != fields.size() --> Warning
//  xii. reader.EndScope();
//  xiii. return Component(typeID, version, fields);
// 
// 5.DeserializeField()
//  i.reader.BeginScope("FIELD");
//  ii.reader.ReadKey("ID");
//  iii.id = reader.ReadValue(Int);
//  iv. reader.ReadKey("Type");
//  v. type = reader.ReadValue(String);
//  vi. reader.ReadKey("Value");
//  vii. value = reader.ReadValue(String);
//  viii. reader.EndScope();
//  //  ix. return Field(id, type, value);
// 
//
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "Serializer/Serialization/Normal/Text/TextReader.h"
#include "Serializer/Serialization/Normal/Text/TextSerializer.h"  // For data structures

class TextDeserializer
{
private:
    TextReader& m_reader;
    std::string m_peekedToken;
    bool m_hasPeekedToken = false;

    // Helper methods
    std::string PeekToken();
    void ConsumePeekedToken();
    void LogWarning(const std::string& message);

    // Private deserialization methods (called recursively)
    SnapshotHeader DeserializeHeader();
    Entity DeserializeEntity();
    Component DeserializeComponent();
    Field DeserializeField();

public:
    TextDeserializer(TextReader& reader);

    // Main public API
    Snapshot DeserializeSnapshot();
};