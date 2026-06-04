//.snapshot.bin --> BinaryReader --> BinarYDeserializer --> NormalDeserializer --> ECS Snapshot
//1. DeserializeSnapshot(reader)
//  i.reader.BeginDocument();
//  ii.reader.BeginScope(SCOPE_SNAPSHOT);
//  iii.header = DeserializeHeader();
//  iv.reader.BeginScope(SCOPE_ENTITIES);
//  v.entities = empty list
//  vi.while reader.HasMoreScopes():
//  vii.   entity = DeserializeEntity();
//  viii.  entities.add(entity);
//  ix.reader.EndScope(); // End Entities
//  x.reader.EndScope(); // End SNAPSHOT
//  xi.return Snapshot(header, entities);
// 
//2. DeserializeHeader()
//  i.reader.BeginScope(SCOPE_HEADER);
//  ii.version = reader.ReadU16();
//  iii.engineHash = reader.ReadU32()
//  iv.timestamp = reader.ReadU64()
//  v.reader.EndScope()
//  vi.Return Header(version , engineHash , timestamp)
// 
//3. DeserializerEntity(reader)
//  i.reader.BeginScope(SCOPE_ENTITY)
//  ii.id = reader.ReadU64()
//  iii.expectedCount = reader.ReadU16()
//  iv.components = empty list
//  v.Repeat expectedCOunt times: 
//		components.add(DeserializeComponent(reader))
//  vi.reader.EndScope()
//  vii.Return EntitySnapshot(id , components)
// 
// 4. DeserializeComponent(reader)
//  i.reader.BeginScope(SCOPE_COMPONENT)
//  ii.typeID = reader.ReadU32()
//  iii.version = reader.ReadU16()
//  iv.expectedCount = reader.ReadU16()
//  v.fields = empty list
//  vi. Repeat expectedCount times : 
//		fields.add(DeserializeField(reader))
//  vii.reader.EndScope()
//  viii.Return ComponentSnapshot(typeID , version , fields)
// 
// 5.DeserializeField(reader)
//  i.reader.BeginScope(SCOPE_FIELD)
//  ii.fieldID = reader.ReadU32()
//  iii.fieldType = reader.ReadU8()
//  iv.Switch(fieldType):
//		-INT --> value = reader.ReadI32()
//		-FLOAT --> value = reader.ReadF32()
//		-VEC3 --> ReadF32() x 3
//		-VEC4 --> ReadF32 x 4
//      -STRING --> reader.ReadString()
//		-BLOB --> reader.ReadU32()
//    data = ReadBytes(size)
//  v.reader.EndScope()
//  vi.Return FieldSnapshot(fieldID , fieldType , value)
// 
//
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "BinaryReader.h"
#include "BinarySerializer.h"  // For scope IDs and data structures

class BinaryDeserializer
{
private:
    BinaryReader& m_reader;

    // Private deserialization methods (called recursively)
    SnapshotHeaderData DeserializeHeader();
    EntityData DeserializeEntity();
    ComponentData DeserializeComponent();
    FieldData DeserializeField();
    void DeserializeFieldValue(FieldData& field, FieldType fieldType);

public:
    BinaryDeserializer(BinaryReader& reader);

    // Main public API
    SnapshotData DeserializeSnapshot();
};