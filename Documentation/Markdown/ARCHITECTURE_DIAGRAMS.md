# Serialization System - Visual Architecture

## 🏗️ High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      GAME APPLICATION                           │
└────────────────┬────────────────────────────────┬────────────────┘
                 │                                │
        ┌────────▼────────┐            ┌─────────▼─────────┐
        │   Save Game     │            │  Network Send     │
        └────────┬────────┘            └─────────┬─────────┘
                 │                                │ 
        ┌────────▼────────────────────────────────▼────────┐
        │   SerializationBridge (Facade)                   │
        │   ├─ CreateSnapshot()                            │
        │   ├─ BuildEntities()                             │
        │   └─ AppendComponents()                          │
        └────────┬─────────────────────────────────────────┘
                 │
        ┌────────▼────────────────────────────────────────┐
        │        ECSSnapshot (In-Memory Staging)          │
        │   ├─ Header (metadata)                          │
        │   ├─ EntitySnapshots[]                          │
        │   │  └─ ComponentSnapshots[]                    │
        │   │     └─ FieldSnapshots[]                     │
        └────────┬──────────────────────┬────────────────┘
                 │                      │
        ┌────────▼──────────┐  ┌───────▼──────────┐
        │  ISerializer      │  │  IDeserializer   │
        │  (abstract)       │  │  (abstract)      │
        └────────┬──────────┘  └───────┬──────────┘
                 │                     │
        ┌────────▼──────────┐  ┌───────▼──────────┐
        │ NormalSerializer  │  │NormalDeserializer│
        │ (binary format)   │  │ (binary format)  │
        └────────┬──────────┘  └───────┬──────────┘
                 │                     │
                 ▼                     ▼
        ┌─────────────────────────────────┐
        │   Binary Buffer (bytes)         │
        │  [0xDEADBEEF][version]...       │
        └──────────┬──────────────────────┘
                   │
        ┌──────────▼──────────┐
        │  Disk / Network     │
        │  game.save          │
        │  network packet     │
        └─────────────────────┘
```

---

## 🔄 Serialization Flow

```
Step 1: Create Snapshot
        ↓
    ECS Manager
    ├─ Entity 1: HealthComponent, TransformComponent, VelocityComponent
    ├─ Entity 2: HealthComponent, TransformComponent
    └─ Entity 3: TransformComponent, VelocityComponent
        ↓
Step 2: SerializationBridge.CreateSnapshot()
        ├─ ClearSnapshot()
        ├─ PopulateHeader()
        ├─ BuildAndAppendEntities()  ← Sorts entities by ID
        │  ├─ BuildAndAppendComponents()  ← Sorts by type ID
        │  │  └─ BuildAndAppendFields()   ← Sorts by field ID
        └─ GetSnapshot()
        ↓
Step 3: NormalSerializer.Serialize()
        ├─ WriteHeader()
        ├─ WriteEntityCount()
        ├─ For each entity (sorted):
        │  ├─ WriteEntity ID
        │  ├─ WriteComponentCount()
        │  └─ For each component (sorted):
        │     ├─ WriteComponentTypeID
        │     ├─ WriteComponentVersion
        │     ├─ WriteFieldCount()
        │     └─ For each field (sorted):
        │        ├─ WriteFieldID
        │        ├─ WriteFieldType
        │        ├─ WriteFieldSize
        │        └─ WriteRawBytes()
        └─ Finalize()
        ↓
Step 4: Binary Buffer (Complete)
        [0xDEADBEEF][versions][header]...[entity1][entity2][entity3]...
```

---

## 🔄 Deserialization Flow

```
Step 1: Read Binary Buffer
        [0xDEADBEEF][versions][header]...[entity1][entity2][entity3]...
        ↓
Step 2: NormalDeserializer.Deserialize()
        ├─ STEP 1: InitializeSnapshot()
        │          → Create empty ECSSnapshot
        │
        ├─ STEP 2: ReadAndValidateHeader()
        │          ├─ Read & validate magic (0xDEADBEEF)
        │          ├─ Read & validate versions
        │          └─ Store in snapshot header
        │
        ├─ STEP 3: ReadEntityCount()
        │          → Reserve entity space
        │
        ├─ STEP 4: For each entity:
        │          ├─ BeginEntity() → Read ID, generation, component count
        │          ├─ For each component:
        │          │  ├─ BeginComponent() → Read type ID, version, field count
        │          │  └─ For each field:
        │          │     └─ ReadField() → ID, type, size, data
        │          └─ EndEntity() → Add to snapshot
        │
        ├─ STEP 5: FinalizeSnapshot()
        │          └─ Validate entity count matches
        │
        └─ STEP 6: Deserialize() complete
                   ↓
Step 3: ECSSnapshot Reconstructed
        ├─ Header (validated)
        ├─ Entities[] (sorted by ID)
        │  └─ Components[] (sorted by type)
        │     └─ Fields[] (sorted by ID)
        ↓
Step 4: ApplyToECS(ecsManager)
        ├─ For each entity:
        │  ├─ Create entity with exact ID
        │  └─ For each component:
        │     ├─ Create component
        │     └─ For each field:
        │        └─ Set field data
        ↓
Step 5: Live ECS Updated
        ECS Manager now has loaded entities!
```

---

## 📊 Data Structure Hierarchy

```
ECSSnapshot
├─ Header
│  ├─ Magic: 0xDEADBEEF
│  ├─ Versions: engine, ecs, schema
│  ├─ Timestamp: 1234567890
│  ├─ SnapshotID: 42
│  ├─ EntityCount: 3
│  ├─ TotalDataSize: 2048 bytes
│  └─ Checksum: 0x12345678
│
└─ EntitySnapshots[] (sorted by ID)
   │
   ├─ EntitySnapshot (ID: 1, Gen: 0)
   │  └─ ComponentSnapshots[] (sorted by type ID)
   │     │
   │     ├─ ComponentSnapshot (Type: 1, Version: 1)
   │     │  └─ FieldSnapshots[] (sorted by field ID)
   │     │     ├─ FieldSnapshot (ID: 1) → Health = 100.0
   │     │     └─ FieldSnapshot (ID: 2) → MaxHealth = 100.0
   │     │
   │     └─ ComponentSnapshot (Type: 2, Version: 1)
   │        └─ FieldSnapshots[] 
   │           ├─ FieldSnapshot (ID: 1) → X = 0.0
   │           ├─ FieldSnapshot (ID: 2) → Y = 0.0
   │           └─ FieldSnapshot (ID: 3) → Z = 0.0
   │
   ├─ EntitySnapshot (ID: 2, Gen: 0)
   │  └─ ComponentSnapshots[] ...
   │
   └─ EntitySnapshot (ID: 3, Gen: 0)
      └─ ComponentSnapshots[] ...
```

---

## 🔀 Interface Hierarchy

```
┌─────────────────────┐         ┌─────────────────────┐
│   ISerializer       │         │  IDeserializer      │
│   (abstract)        │         │  (abstract)         │
├─────────────────────┤         ├─────────────────────┤
│ + Serialize()       │         │ + Deserialize()     │
│ + SerializeToBuffer │         │ + DeserializeFrom.. │
│ + SerializeToFile   │         │ + GetLastError()    │
│ + GetLastError()    │         │ + GetBytesRead()    │
│ + GetBytesWritten() │         │ + WasSuccessful()   │
│ + WasSuccessful()   │         └─────────────────────┘
└──────────┬──────────┘                    ▲
           │                               │
           ├─────────────────────┬─────────┘
           │                     │
           ▼                     ▼
┌──────────────────┐   ┌──────────────────┐
│ Normal           │   │ Normal           │
│ Serializer       │   │ Deserializer     │
│ (Binary)         │   │ (Binary)         │
└──────────────────┘   └──────────────────┘
           │                     │
           ├─ Compressed         ├─ Compressed
           ├─ JSON               ├─ JSON
           └─ Network            └─ Network
```

---

## ⚙️ Component Flow

```
Schema Registry
├─ ComponentSchema for HealthComponent
│  ├─ Type ID: 1
│  ├─ Name: "HealthComponent"
│  ├─ Size: 12 bytes
│  └─ Fields:
│     ├─ Field 1: health (offset: 0, size: 4)
│     ├─ Field 2: maxHealth (offset: 4, size: 4)
│     └─ Field 3: isDead (offset: 8, size: 1)
│
├─ ComponentSchema for TransformComponent
│  ├─ Type ID: 2
│  ├─ Name: "TransformComponent"
│  ├─ Size: 24 bytes
│  └─ Fields:
│     ├─ Field 1: x (offset: 0, size: 4)
│     ├─ Field 2: y (offset: 4, size: 4)
│     ├─ Field 3: z (offset: 8, size: 4)
│     ├─ Field 4: rotationX (offset: 12, size: 4)
│     ├─ Field 5: rotationY (offset: 16, size: 4)
│     └─ Field 6: rotationZ (offset: 20, size: 4)
│
└─ ComponentSchema for VelocityComponent
   ├─ Type ID: 3
   ├─ Name: "VelocityComponent"
   ├─ Size: 12 bytes
   └─ Fields:
      ├─ Field 1: vx (offset: 0, size: 4)
      ├─ Field 2: vy (offset: 4, size: 4)
      └─ Field 3: vz (offset: 8, size: 4)
```

---

## 📈 Byte Layout Example

Binary snapshot with 1 entity (2 components):

```
Offset  Value           Description
─────────────────────────────────────────
0x0000  0xDEADBEEF      Magic number
0x0004  0x010000        Engine version
0x0008  0x020100        ECS version
0x000C  0x00000001      Schema version
0x0010  [8 bytes]       Timestamp
0x0018  0x0000002A      Snapshot ID (42)
0x001C  0x00000001      Entity count (1)
0x0020  [8 bytes]       Total data size
0x0028  [4 bytes]       Checksum
0x002C  [entity data]   →

        ┌─ Entity Data
0x0030  0x00000001      Entity ID: 1
0x0034  0x00000000      Generation: 0
0x0038  0x00000002      Component count: 2

        ┌─ Component 1 (Health)
0x003C  0x00000001      Component Type ID: 1
0x0040  0x00000001      Component Version: 1
0x0044  0x00000003      Field count: 3

        ┌─ Field 1 (health)
0x0048  0x00000001      Field ID: 1
0x004C  0x05            Field Type: FLOAT (5)
0x004D  [padding]
0x004E  0x00000004      Field Size: 4
0x0052  0x42C80000      Field Data: 100.0 (float)

        ┌─ Field 2 (maxHealth)
0x0056  0x00000002      Field ID: 2
0x005A  0x05            Field Type: FLOAT (5)
0x005B  [padding]
0x005C  0x00000004      Field Size: 4
0x0060  0x42C80000      Field Data: 100.0 (float)

        ┌─ Field 3 (isDead)
0x0064  0x00000003      Field ID: 3
0x0068  0x08            Field Type: BOOL (8)
0x0069  [padding]
0x006A  0x00000001      Field Size: 1
0x006E  0x00            Field Data: false

        ┌─ Component 2 (Transform)
0x006F  0x00000002      Component Type ID: 2
0x0073  0x00000001      Component Version: 1
0x0077  0x00000006      Field count: 6

        └─ [similar field structure for 6 float fields]
```

---

## 🎯 Validation Pipeline

```
Input Buffer
    ↓
STEP 1: Validate Magic Number
        ├─ Read 0xDEADBEEF
        └─ Reject if mismatch → "Invalid magic number"
    ↓
STEP 2: Validate Format Version
        ├─ Read version
        └─ Reject if incompatible → "Version mismatch"
    ↓
STEP 3: Validate Entity Count
        ├─ Read count
        ├─ Check: 0 < count ≤ 1,000,000
        └─ Reject if out of range → "Entity count too large"
    ↓
STEP 4: Validate Component Count (per entity)
        ├─ Read count
        ├─ Check: 0 < count ≤ 256
        └─ Reject if out of range → "Component count too large"
    ↓
STEP 5: Validate Field Count (per component)
        ├─ Read count
        ├─ Check: 0 < count ≤ 256
        └─ Reject if out of range → "Field count too large"
    ↓
STEP 6: Validate Field Size
        ├─ Read size
        ├─ Check: 0 < size ≤ 10 MB
        └─ Reject if out of range → "Field size too large"
    ↓
STEP 7: Validate Entity Count Matches Header
        ├─ Count entities read
        ├─ Compare with header entity count
        └─ Reject if mismatch → "Entity count mismatch"
    ↓
Valid Snapshot!
```

---

## 🚀 Test Program Flow

```
Start
  ├─ Register Component Schemas
  │  ├─ HealthComponent
  │  ├─ TransformComponent
  │  └─ VelocityComponent
  │
  ├─ Create Test ECS
  │  ├─ Entity 1: Player (Health, Transform, Velocity)
  │  ├─ Entity 2: Enemy (Health, Transform)
  │  └─ Entity 3: Projectile (Transform, Velocity)
  │
  ├─ Serialize to Buffer
  │  ├─ NormalSerializer.Serialize()
  │  └─ Output: 1024 bytes
  │
  ├─ Deserialize from Buffer
  │  ├─ NormalDeserializer.Deserialize()
  │  └─ Populate new ECS
  │
  ├─ Roundtrip Serialization
  │  ├─ Serialize again
  │  └─ Output: 1024 bytes
  │
  ├─ Verify Determinism
  │  ├─ Compare: Buffer 1 == Buffer 2?
  │  └─ ✓ All bytes match!
  │
  └─ SUCCESS!
```

---

**This visual architecture shows how all pieces fit together!** 🎨

You can use these diagrams in documentation, presentations, or team discussions. 📊
