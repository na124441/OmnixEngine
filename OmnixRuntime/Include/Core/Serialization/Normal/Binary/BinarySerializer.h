//   ECS Snapshot --> NormalSerializer --> BinarySerializer --> BinaryWriter --> .snapshot.bin
// It writes stable numeric IDS: 
//   SCOPE_SNAPSHOT = 1
//   SCOPE_HEADER = 2
//   SCOPE_ENTITIES = 3
//   SCOPE_ENTITY = 4
//   SCOPE_COMPONENT = 5
//   SCOPE_FIELD = 6
// 
// 1.SerializeSnapshot(snapshot, writer)
//   i.writer.BeginDocument();
//   ii.writer.BeginScope(SCOPE_SNAPSHOT);
//   iii.SerializeHeader(snapshot.header);
//   iv.writer.BeginScope(SCOPE_ENTITIES);
//   v.For each entity in snapshot.entities:
//		SerializeEntity(entity);
//   vi. writer.EndScope(); // End Entities
//   vii. writer.EndScope(); // End SNAPSHOT
//   viii. writer.EndDocument();
// 
// 2.SerializeHeader(header)
//  i.writer.BeginScope(SCOPE_HEADER);
//  ii.writer.U16(header.version);
//  iii.writer.U32(header.engineName.length());
//  iv.writer.WriteBytes(header.engineName.data(), header.engineName.length());
//  v.writer.U64(header.timestamp);
//  vi. writer.EndScope();
//  
// 3.SerializeEntity(entity)
//  i.witer.BeginScope(SCOPE_ENTITY);
//  ii.writer.U64(entity.id);
//  iii.writer.U16(entity.GetComponentCount());
//  iv.For each component in entity.components:
// //	      SerializeComponent(component);
//  v. writer.EndScope();'
// 
// 4.SerializeComponent(component)
//  i.writer.BeginScope(SCOPE_COMPONENT);
//  ii.writer.U32(component.typeID);
//  iii.writer.U16(component.version);
//  iv.writer.U16(component.GetFieldCount());
//  v.For each field in component.fields:
//		SerializeField(field);
//  vi. writer.EndScope();
// 
// 5.SerializeField(field)
//  i.writer.BeginScope(SCOPE_FIELD);
//  ii.writer.U32(field.id);
//  iii.writer.U8(field.type.length());
//  iv.Switch(field.type):
//    -INT --> writer.I32(value)
//    -FLOAT --> writer.F32(value)
//    -VEC3 --> writer.F32(x), writer.F32(y), writer.F32(z)
//    -VEC4 --> writer.F32(x), writer.F32(y), writer.F32(z), writer.F32(w)
//    -STRING --> writer.U32(value.length()), writer.WriteBytes(value.data(), value.length());
//    -BLOB --> writer.U32(value.size()), writer.WriteBytes(value.data(), value.size());
//  WriteBytes(data)
//  v.writer.EndScope();
// 
// 
// 
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Core/Serialization/Normal/Binary/BinaryWriter.h"
#include "Serializer/ECS/FieldDelta.h"

// Stable scope IDs
constexpr uint32_t SCOPE_SNAPSHOT = 1;
constexpr uint32_t SCOPE_HEADER = 2;
constexpr uint32_t SCOPE_ENTITIES = 3;
constexpr uint32_t SCOPE_ENTITY = 4;
constexpr uint32_t SCOPE_COMPONENT = 5;
constexpr uint32_t SCOPE_FIELD = 6;

struct ComponentData
{
    uint32_t typeID;
    uint16_t version;
    std::vector<FieldData> fields;

    ComponentData() : typeID(0), version(1) {}

    uint16_t GetFieldCount() const { return static_cast<uint16_t>(fields.size()); }
};

struct EntityData
{
    uint64_t id;
    std::vector<ComponentData> components;

    EntityData() : id(0) {}

    uint16_t GetComponentCount() const { return static_cast<uint16_t>(components.size()); }
};

struct SnapshotHeaderData
{
    uint16_t version;
    std::string engineName;
    uint64_t timestamp;

    SnapshotHeaderData() : version(1), engineName(""), timestamp(0) {}
};

struct SnapshotData
{
    SnapshotHeaderData header;
    std::vector<EntityData> entities;

    SnapshotData() {}
};

class BinarySerializer
{
private:
    BinaryWriter& m_writer;

    // Private serialization methods (called recursively)
    void SerializeHeader(const SnapshotHeaderData& header);
    void SerializeEntity(const EntityData& entity);
    void SerializeComponent(const ComponentData& component);
    void SerializeField(const FieldData& field);
    void SerializeFieldValue(const FieldData& field);

public:
    BinarySerializer(BinaryWriter& writer);

    // Main public API
    void SerializeSnapshot(const SnapshotData& snapshot);
};
