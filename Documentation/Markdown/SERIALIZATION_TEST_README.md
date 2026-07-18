# Serialization Module Test

This test program validates the entire serialization pipeline with visible results.

## What It Tests

✅ Component schema registration  
✅ ECS entity and component creation  
✅ Serialization to binary format (NormalSerializer)  
✅ Deserialization from binary (NormalDeserializer)  
✅ Roundtrip serialization (serialize → deserialize → serialize)  
✅ Deterministic serialization verification  
    
## Expected Output

```
╔════════════════════════════════════════╗
║   SERIALIZATION MODULE TEST PROGRAM    ║
║   Testing NormalSerializer/Deserializer║
╚════════════════════════════════════════╝

========================================
Registering Component Schemas
========================================
✓ HealthComponent schema registered
✓ TransformComponent schema registered
✓ VelocityComponent schema registered

========================================
Creating Test ECS State
========================================
  Created Entity #1 (Player)
  ✓ Added HealthComponent to Entity #1
  ✓ Added TransformComponent to Entity #1
  ✓ Added VelocityComponent to Entity #1
  Created Entity #2 (Enemy)
  ✓ Added HealthComponent to Entity #2
  ✓ Added TransformComponent to Entity #2
  Created Entity #3 (Projectile)
  ✓ Added TransformComponent to Entity #3
  ✓ Added VelocityComponent to Entity #3

  Total entities created: 3
✓ Test ECS state created successfully

========================================
Testing Serialization (NormalSerializer)
========================================
✓ SerializationBridge initialized
✓ SnapshotContext created (SNAPSHOT_SAVE)
✓ ECSSnapshot created
────────────────────────────────────────
  Snapshot ID          : 1
  Entity Count         : 3
  Total Data Size      : 1024 bytes
────────────────────────────────────────
✓ NormalSerializer created
✓ Snapshot serialized to buffer
────────────────────────────────────────
  Bytes written        : 1024
  Buffer size          : 1024
  Format version       : 1
────────────────────────────────────────

========================================
Testing Deserialization (NormalDeserializer)
========================================
✓ NormalDeserializer created
✓ Snapshot deserialized successfully
────────────────────────────────────────
  Bytes read           : 1024
  Entities deserialized: 3
  Components deserialized: 8
  Fields deserialized  : 20
────────────────────────────────────────

========================================
Testing Serialization Roundtrip
========================================
✓ Roundtrip snapshot created
✓ Roundtrip serialization completed

========================================
Verifying Serialization Roundtrip
========================================
✓ Buffer sizes match
✓ All bytes match (deterministic serialization verified!)
────────────────────────────────────────
  Total bytes          : 1024
  Match percentage     : 100%
────────────────────────────────────────

========================================
TEST SUMMARY
========================================
✓ Component schema registration
✓ Test ECS creation
✓ Serialization (NormalSerializer)
✓ Deserialization (NormalDeserializer)
✓ Roundtrip serialization
✓ Deterministic verification

╔════════════════════════════════════════╗
║   ✓ ALL TESTS PASSED!                  ║
║   Serialization module is working!     ║
╚════════════════════════════════════════╝
```

## Build Instructions

### Linux/macOS

```bash
chmod +x build_and_test.sh
./build_and_test.sh
```

### Windows

```cmd
build_and_test.bat
```

### Manual Build (any platform)

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
./serialization_test          # Linux/macOS
Release\serialization_test    # Windows
```

## What Happens

1. **Schema Registration**: Registers 3 test components (Health, Transform, Velocity)
2. **ECS Creation**: Creates 3 test entities with various components
3. **Serialization**: Uses NormalSerializer to write the ECS state to binary
4. **Deserialization**: Uses NormalDeserializer to read the binary back
5. **Roundtrip**: Serializes the deserialized state again
6. **Verification**: Compares original and roundtrip bytes (must be identical!)

## Key Results to Look For

✅ **Bytes written = Bytes read** → Serialization is complete  
✅ **Entities/Components/Fields match** → All data captured  
✅ **All bytes match** → Deterministic serialization (same data = same bytes)  
✅ **Zero errors** → No corruption or validation issues  

## If Tests Fail

Check:
- Component schema offsets are correct
- Component sizes match actual struct sizes
- Field types are correctly mapped
- Buffer allocation succeeded
- No null pointers in data structures

## Architecture

```
Test Program
    ↓
Create ECS + Components
    ↓
Serialize (NormalSerializer)
    ↓ Binary Buffer
Deserialize (NormalDeserializer)
    ↓
Create new ECS
    ↓
Serialize again (Roundtrip)
    ↓ Compare bytes
All match? ✓ SUCCESS
```

## Next Steps

Once tests pass:
1. Implement CompressedSerializer for smaller files
2. Add network serialization with delta updates
3. Implement JSON serializer for debugging
4. Add save/load game functionality
5. Implement networked snapshot replication

Enjoy! 🚀
