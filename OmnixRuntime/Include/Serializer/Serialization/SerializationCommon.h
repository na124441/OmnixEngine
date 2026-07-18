#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <algorithm>

// ============================================================================
// SERIALIZATION COMMON - Centralized rules for deterministic serialization
// ============================================================================
//
// PURPOSE:
//   Single source of truth for serialization rules
//   Prevents different serializers from producing different bytes
//   Ensures consistency across all formats (Binary, JSON, XML, etc.)
//
// PROBLEM SOLVED:
//   Without this, different serializers might:
//   - Sort entities differently (ID vs generation vs name)
//   - Order components differently (type ID vs insertion order)
//   - Write fields in different order (ID vs memory offset)
//   - Handle nulls inconsistently (skip vs write marker)
//   Result: Same snapshot → Different bytes depending on serializer
//
// SOLUTION:
//   All serializers follow SAME rules defined here
//   Any serializer can be swapped without changing behavior
//   Validation is consistent across formats
//
// USAGE:
//   Every serializer includes this header
//   Uses constants and rules from SerializationCommon
//   All serializers produce identical byte order for same data
//
// ============================================================================

namespace SerializationCommon {

// ============================================================================
// SECTION 1: MAGIC NUMBERS & VALIDATION MARKERS
// ============================================================================

/// Magic number for ECS snapshot validation
/// Used to detect corrupted files and verify format
/// Must be first bytes written, first bytes checked on load
constexpr uint32_t ECS_SNAPSHOT_MAGIC = 0xDEADBEEF;

/// Version of serialization format
/// Increment when changing serialization layout
/// Used for compatibility checking
constexpr uint32_t SERIALIZATION_FORMAT_VERSION = 1;

/// Magic number for file format (optional, alternate magic)
/// Can use if need multiple magic numbers
constexpr uint32_t BINARY_FORMAT_MAGIC = 0xCAFEBABE;

// ============================================================================
// SECTION 2: ENTITY ORDERING RULES
// ============================================================================

/// RULE: Entities MUST be serialized in this order
/// Default: Sorted by Entity ID (ascending)
/// Rationale:
///   - Deterministic (same order every time)
///   - Predictable (ID 1, 2, 3, ...)
///   - Efficient (IDs usually sequential)
///
/// IMPLEMENTATION:
///   std::vector<uint32_t> entities = GetAllEntities();
///   std::sort(entities.begin(), entities.end());  // Sort by ID
///   for (uint32_t id : entities) {
///       WriteEntity(id);
///   }
///
inline bool ShouldEntityAPrecedeEntityB(uint32_t entityA, uint32_t entityB) {
    return entityA < entityB;  // Sort by ID ascending
}

/// Get entity sort comparator
inline std::function<bool(uint32_t, uint32_t)> GetEntityComparator() {
    return [](uint32_t a, uint32_t b) {
        return ShouldEntityAPrecedeEntityB(a, b);
    };
}

/// DOCUMENTATION: Entity Ordering
/// - Primary sort: Entity ID (ascending)
/// - Never sort by generation (changes over time)
/// - Never sort by component count
/// - Never shuffle order randomly
constexpr const char* ENTITY_ORDERING_RULE = 
    "Sort entities by ID ascending (1, 2, 3, ...)";

// ============================================================================
// SECTION 3: COMPONENT ORDERING RULES
// ============================================================================

/// RULE: Components within entity MUST be serialized in this order
/// Default: Sorted by Component Type ID (ascending)
/// Rationale:
///   - Deterministic (same order every time)
///   - Efficient (cache locality)
///   - Independent of insertion order
///
/// IMPLEMENTATION:
///   auto components = entity.GetComponents();
///   std::sort(components.begin(), components.end(),
///       [](const auto& a, const auto& b) {
///           return a.GetComponentTypeID() < b.GetComponentTypeID();
///       });
///   for (auto& component : components) {
///       WriteComponent(component);
///   }
///
inline bool ShouldComponentAPrecedeComponentB(uint32_t typeA, uint32_t typeB) {
    return typeA < typeB;  // Sort by type ID ascending
}

/// Get component sort comparator
inline std::function<bool(uint32_t, uint32_t)> GetComponentComparator() {
    return [](uint32_t a, uint32_t b) {
        return ShouldComponentAPrecedeComponentB(a, b);
    };
}

/// DOCUMENTATION: Component Ordering
/// - Primary sort: Component Type ID (ascending)
/// - Never sort by version
/// - Never sort by field count
/// - Never shuffle order randomly
constexpr const char* COMPONENT_ORDERING_RULE = 
    "Sort components by Type ID ascending (1, 2, 3, ...)";

// ============================================================================
// SECTION 4: FIELD ORDERING RULES
// ============================================================================

/// RULE: Fields within component MUST be serialized in this order
/// Default: Sorted by Field ID (ascending)
/// Rationale:
///   - Deterministic (same order every time)
///   - Independent of memory layout
///   - Independent of schema definition order
///
/// IMPLEMENTATION:
///   auto fields = component.GetFields();
///   std::sort(fields.begin(), fields.end(),
///       [](const auto& a, const auto& b) {
///           return a.GetFieldID() < b.GetFieldID();
///       });
///   for (auto& field : fields) {
///       WriteField(field);
///   }
///
inline bool ShouldFieldAPrecedeFieldB(uint32_t fieldA, uint32_t fieldB) {
    return fieldA < fieldB;  // Sort by field ID ascending
}

/// Get field sort comparator
inline std::function<bool(uint32_t, uint32_t)> GetFieldComparator() {
    return [](uint32_t a, uint32_t b) {
        return ShouldFieldAPrecedeFieldB(a, b);
    };
}

/// DOCUMENTATION: Field Ordering
/// - Primary sort: Field ID (ascending)
/// - Never sort by offset (memory layout changes)
/// - Never sort by type
/// - Never shuffle order randomly
constexpr const char* FIELD_ORDERING_RULE = 
    "Sort fields by ID ascending (1, 2, 3, ...)";

// ============================================================================
// SECTION 5: NULL/EMPTY HANDLING RULES
// ============================================================================

/// RULE: How to handle null/empty values
/// Option 1: Skip them (don't write)
/// Option 2: Write marker + skip data
/// Option 3: Write as zero/empty
///
/// CHOSEN: Write as zero/empty
/// Rationale:
///   - Simpler (no markers needed)
///   - Fixed size (easier to parse)
///   - Predictable (always same bytes)
///   - Deterministic (no special cases)

/// Check if null pointer should be written
/// RULE: Write nulls as zero data
inline bool ShouldWriteNullPointer() {
    return true;  // YES, write nulls
}

/// Check if empty container should be written
/// RULE: Write empties as zero count
inline bool ShouldWriteEmptyContainer() {
    return true;  // YES, write empty containers
}

/// DOCUMENTATION: Null Handling
/// - Don't skip null pointers (write marker: count=0 or size=0)
/// - Don't use special null markers (complicates parsing)
/// - Write nulls consistently across all serializers
/// - Deserializer can check count/size to detect empty
constexpr const char* NULL_HANDLING_RULE = 
    "Write nulls/empty as zero count/size, no special markers";

// ============================================================================
// SECTION 6: BYTE ORDER RULES (Endianness)
// ============================================================================

/// RULE: Default byte order is little-endian
/// Rationale:
///   - Most common on modern systems (x86, ARM)
///   - Network standard on modern hardware
///   - JavaScript/WebAssembly native
///   - Can always be overridden in context
///
/// IMPORTANT:
///   Serializers can support other byte orders via context
///   But DEFAULT must be little-endian for consistency
///
constexpr bool DEFAULT_LITTLE_ENDIAN = true;

/// Convert value to little-endian bytes
inline uint32_t HostToLittleEndian32(uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (value & 0xFF);
    bytes[1] = ((value >> 8) & 0xFF);
    bytes[2] = ((value >> 16) & 0xFF);
    bytes[3] = ((value >> 24) & 0xFF);
    return *reinterpret_cast<uint32_t*>(bytes);
}

inline uint64_t HostToLittleEndian64(uint64_t value) {
    uint8_t bytes[8];
    bytes[0] = (value & 0xFF);
    bytes[1] = ((value >> 8) & 0xFF);
    bytes[2] = ((value >> 16) & 0xFF);
    bytes[3] = ((value >> 24) & 0xFF);
    bytes[4] = ((value >> 32) & 0xFF);
    bytes[5] = ((value >> 40) & 0xFF);
    bytes[6] = ((value >> 48) & 0xFF);
    bytes[7] = ((value >> 56) & 0xFF);
    return *reinterpret_cast<uint64_t*>(bytes);
}

/// DOCUMENTATION: Byte Order
/// - Primary: Little-endian (default)
/// - Alternative: Big-endian (via context)
/// - Alternative: Native (fastest, not portable)
/// - All must be consistent within one snapshot
constexpr const char* BYTE_ORDER_RULE = 
    "Default: Little-endian, can be overridden via context";

// ============================================================================
// SECTION 7: SERIALIZATION ORDER RULES
// ============================================================================

/// RULE: Data must be written in this exact order
/// This is THE deterministic order that all serializers must follow
///
/// ORDER:
/// 1. HEADER
///    - Magic (0xDEADBEEF)
///    - Engine version
///    - ECS version
///    - Schema version
///    - Timestamp
///    - Snapshot ID
///    - Entity count
///    - Total size
///    - Checksum
///    - Snapshot type
///
/// 2. ENTITIES (sorted by ID ascending)
///    - Entity ID
///    - Entity generation
///    - Component count
///
/// 3. COMPONENTS (for each entity, sorted by Type ID ascending)
///    - Component Type ID
///    - Component version
///    - Field count
///
/// 4. FIELDS (for each component, sorted by ID ascending)
///    - Field ID
///    - Field type
///    - Field size
///    - Raw data (size bytes)
///
/// CRITICAL:
///   ALL serializers MUST follow this order exactly
///   No deviations
///   No optimizations that change order
///   No skipping steps
///
constexpr const char* SERIALIZATION_ORDER = 
    "Header → Entities (sorted) → Components (sorted) → Fields (sorted)";

// ============================================================================
// SECTION 8: VALIDATION RULES
// ============================================================================

/// RULE: How to validate serialized data
/// Checklist that all deserializers must perform

/// Step 1: Validate magic number
inline bool ValidateMagic(uint32_t magic) {
    return magic == ECS_SNAPSHOT_MAGIC;
}

/// Step 2: Validate format version
inline bool ValidateFormatVersion(uint32_t version, uint32_t minimumRequired) {
    return version >= minimumRequired;
}

/// Step 3: Validate entity count (not too many)
inline bool ValidateEntityCount(uint32_t count) {
    constexpr uint32_t MAX_ENTITIES = 1000000;  // 1 million max
    return count > 0 && count <= MAX_ENTITIES;
}

/// Step 4: Validate component count per entity
inline bool ValidateComponentCount(uint32_t count) {
    constexpr uint32_t MAX_COMPONENTS_PER_ENTITY = 256;
    return count > 0 && count <= MAX_COMPONENTS_PER_ENTITY;
}

/// Step 5: Validate field count per component
inline bool ValidateFieldCount(uint32_t count) {
    constexpr uint32_t MAX_FIELDS_PER_COMPONENT = 256;
    return count > 0 && count <= MAX_FIELDS_PER_COMPONENT;
}

/// Step 6: Validate field size (not too large)
inline bool ValidateFieldSize(uint32_t size) {
    constexpr uint32_t MAX_FIELD_SIZE = 10485760;  // 10 MB per field
    return size > 0 && size <= MAX_FIELD_SIZE;
}

/// DOCUMENTATION: Validation Rules
/// - Always validate magic first (quick reject)
/// - Always validate counts (prevent out-of-memory)
/// - Always validate sizes (prevent buffer overflow)
/// - Validation level determines how strict to be
constexpr const char* VALIDATION_RULE = 
    "Magic → Version → Counts → Sizes (in order)";

// ============================================================================
// SECTION 9: CHECKSUM RULES (Optional)
// ============================================================================

/// RULE: If writing checksum, use CRC32    
/// Rationale:
///   - Fast to compute
///   - Good for detecting transmission errors
///   - Not cryptographic (that's encryption's job)
///
/// IMPLEMENTATION:
///   Calculate CRC32 of all data written
///   Write CRC32 in header
///   On load, recalculate and compare
///

/// Calculate CRC32 checksum
inline uint32_t CalculateCRC32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;

    // Standard CRC32 polynomial
    constexpr uint32_t POLYNOMIAL = 0xEDB88320;

    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];

        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ POLYNOMIAL;
            } else {
                crc = (crc >> 1);
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

/// DOCUMENTATION: Checksum Rules
/// - Use CRC32 for error detection
/// - Include all data in checksum (except checksum itself)
/// - Write checksum in header
/// - Optional (context controls)
constexpr const char* CHECKSUM_RULE = 
    "Use CRC32 if WriteChecksum enabled in context";

} // namespace SerializationCommon
