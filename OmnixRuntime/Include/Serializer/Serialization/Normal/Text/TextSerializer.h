//1. SerializerSnapshot(snapshot.writer)
//	i.writer.BeginDocument();
//  ii.writer.BeginScope("SNAPSHOT");
//  iii.SerializerHeader(snapshot.header);
//  iv. writer.BeginScope("Entities);
//  v. For each entity in snapshot.entities:
//		SerializeEntity(entity);
//  vi. writer.EndScope(); // End Entities
//  vii. writer.EndScope(); // End SNAPSHOT
//  viii. writer.EndDocument();
// 
// 2.SerializeHeader(header)
//  i.writer.BeginScope("HEADER");
//  ii.writer.WriteKey("Version");
//  iii.writer.WriteValue(header.version);
//  iv.writer.WriteNewline();
//  v.writer.WriteKey("Engine");
//  vi.writer.WriteValue(header.engineName);
//  vii.writer.WrtieNewline();
//  viii.writer.WriteKey("Timestamp");
//  ix.writer.WriteValue(header.timestamp);
//  x.writer.WriteNewline();
//  xi. writer.EndScope();
// 
// 3.SerializeEntity(entity)
//  i.writer.BeginScope("ENTITY");
//  ii.writer.WriteKey("ID");
//  iii.writer.WriteValue(entity.id);
//  iv.writer.WriteNewline();
//  v.writer.WriteKey("ComponentCount");
//  vi.writer.WriteValue(entity.componentCount);
//  vii.writer.WriteNewline();
//  viii.For each component in entity.components: 
//	      SerializeComponent(component);
//  ix. writer.EndScope();
// 
// 4.SerializeComponent(component)
//  i.writer.BeginScope("COMPONENT");
//  ii.writer.WriteKey("Type");
//  iii.writer.WriteValue(component.typeID);
//  iv. writer.WriteNewline();
//  v.writer.WriteKey("Version");
//  vi.writer.WriteValue(component.version);
//  vii.writer.WriteNewline();
//  viii.writer.WriteKey("Field_Count");
//  ix.writer.WriteValue(component.fieldCount);
//  x.writer.WriteNewline();
//  xi.For each field in component.fields:
//        SerializeField(field);
// xii. writer.EndScope();
// 
//5. SerializeField(field)
//  i.writer.BeginScope("FIELD");
//  ii.writer.WriteKey("ID");
//  iii.writer.WriteValue(field.id);
//  iv. writer.WriteNewline();
//  v.writer.WriteKey("Type");
//  vi.writer.WriteValue(field.type);
//  vii.writer.WriteNewline();
//  viii.writer.WriteKey("Value");
//  ix.writer.WriteValue(field.value);
//  x.writer.WriteNewline();
//  xi. writer.EndScope();
// 
// 
// 
//
#pragma once

#include <string>
#include <vector>
#include <ctime>
#include "Serializer/Serialization/Normal/Text/TextWriter.h"

// Data structures for serialization

struct SnapshotHeader
{
    int version;
    std::string engineName;
    long long timestamp;

    SnapshotHeader() : version(1), engineName(""), timestamp(0) {}
};

struct Field
{
    int id;
    std::string type;
    std::string value;

    Field() : id(0), type(""), value("") {}
    Field(int id, const std::string& type, const std::string& value)
        : id(id), type(type), value(value) {}
};

struct Component
{
    int typeID;
    int version;
    std::vector<Field> fields;

    Component() : typeID(0), version(1) {}

    int GetFieldCount() const { return static_cast<int>(fields.size()); }
};

struct Entity
{
    int id;
    std::vector<Component> components;

    Entity() : id(0) {}

    int GetComponentCount() const { return static_cast<int>(components.size()); }
};

struct Snapshot
{
    SnapshotHeader header;
    std::vector<Entity> entities;

    Snapshot() {}
};

class TextSerializer
{
private:
    TextWriter& m_writer;

    // Private serialization methods (called recursively)
    void SerializeHeader(const SnapshotHeader& header);
    void SerializeEntity(const Entity& entity);
    void SerializeComponent(const Component& component);
    void SerializeField(const Field& field);

public:
    TextSerializer(TextWriter& writer);

    // Main public API
    void SerializeSnapshot(const Snapshot& snapshot);
};