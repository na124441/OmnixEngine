#pragma once

#include <stdexcept>
#include <vector>
#include <cstdint>
#include <memory>

// Forward declarations
class ECSSnapshot;
class ECS;
class SerializerContext;
class ComponentSchemaRegistry;

// ============================================================================
// INTERFACE: IDeserializer - Abstract base for all snapshot deserializers
// ============================================================================
//
// PURPOSE:
//   Read binary data and reconstruct ECS state into a live ECS
//   Reverse operation of ISerializer
//   More complex than serialization because of:
//     - Version compatibility checking
//     - Schema migration
//     - Entity creation/reuse
//     - Reference resolution
//
// ALGORITHM:
//   Step 1: Read & Validate Header
//     - Magic number check
//     - Version compatibility
//     - Snapshot type
//
//   Step 2: Read Entity Descriptors (staging)
//     - Entity count
//     - Component count per entity
//     - All metadata (no data yet)
//     - Stored in temporary structure
//
//   Step 3: Entity Instantiation
//     - Create entities with exact IDs
//     - Handle existing entities (reuse/destroy based on context)
//     - Register in ECS
//
//   Step 4: Component Creation
//     - For each entity, create components
//     - Validate schema exists
//     - Check version compatibility
//     - Do NOT set field values yet
//
//   Step 5: Field Assignment
//     - Read raw field data
//     - Convert versions if needed
//     - Copy bytes into component memory
//     - Skip unknown/transient fields
//
//   Step 6: Reference Resolution
//     - Scan for entity references
//     - Validate target entities exist
//     - Fix internal links
//     - Handle missing targets (null/default/error)
//
// COMPLEXITY:
//   Deserialization is 2-3x harder than serialization because:
//   - Must validate everything
//   - Must handle partial loads
//   - Must handle schema mismatches
//   - Must resolve references
//   - Must manage entity lifecycle
//
// ============================================================================

class IDeserializer {
public:
    // ========================================================================
    // VIRTUAL DESTRUCTOR (required for polymorphism)
    // ========================================================================

    virtual ~IDeserializer() = default;

    // ========================================================================
    // PRIMARY INTERFACE - Core deserialization pipeline
    // ========================================================================

    /// Deserialize from binary buffer to ECS
    ///
    /// INPUT:
    ///   - data: Binary buffer containing serialized snapshot
    ///   - size: Size of buffer in bytes
    ///   - ecsManager: Target ECS to reconstruct into
    ///   - registry: Schema registry for component definitions
    ///
    /// PROCESS (6 steps):
    ///   1. Read & Validate Header
    ///      - Check magic number (0xDEADBEEF)
    ///      - Verify version compatibility
    ///      - Check snapshot type matches context
    ///
    ///   2. Read Entity Descriptors (staging phase)
    ///      - Read all entity metadata
    ///      - Read component descriptors
    ///      - Store in temporary structure
    ///      - Do NOT create entities yet
    ///
    ///   3. Entity Instantiation
    ///      - Create entities with exact IDs from snapshot
    ///      - Handle conflicts:
    ///        * REUSE: Keep existing entity
    ///        * DESTROY: Delete existing, create new
    ///        * SKIP: Don't load this entity
    ///      - Context determines strategy
    ///
    ///   4. Component Creation
    ///      - For each entity, add components
    ///      - Validate component schema exists
    ///      - Validate version compatibility
    ///      - Do NOT set field values yet
    ///
    ///   5. Field Assignment
    ///      - Read raw field data
    ///      - Find field in schema
    ///      - Convert version if needed
    ///      - Copy bytes directly to component memory
    ///      - Skip unknown fields
    ///
    ///   6. Reference Resolution
    ///      - Scan for entity references
    ///      - Validate targets exist
    ///      - Fix internal links
    ///      - Handle missing targets
    ///
    /// RETURNS:
    ///   - true: Deserialization successful
    ///   - false: Failed at some step, check GetLastError()
    ///
    /// REQUIREMENTS:
    ///   - Buffer must be valid (not nullptr, size > 0)
    ///   - ECS must be provided (non-null)
    ///   - Registry must have all component schemas
    ///   - Must not throw exceptions
    ///
    virtual bool Deserialize(const uint8_t* data,
                            size_t size,
                            ECS& ecsManager,
                            const ComponentSchemaRegistry& registry) = 0;

    // ========================================================================
    // INPUT VARIANTS - Different input sources
    // ========================================================================

    /// Deserialize from memory buffer
    ///
    /// INPUT:
    ///   - buffer: Vector of bytes to deserialize
    ///   - ecsManager: Target ECS
    ///   - registry: Component schema registry
    ///
    /// RETURNS:
    ///   - true: Success
    ///   - false: Failed, check GetLastError()
    ///
    virtual bool DeserializeFromBuffer(const std::vector<uint8_t>& buffer,
                                      ECS& ecsManager,
                                      const ComponentSchemaRegistry& registry) {

        if (buffer.empty()) {
            return false;
        }

        return Deserialize(buffer.data(), buffer.size(), ecsManager, registry);
    }

    /// Deserialize from file
    ///
    /// IMPORTANT:
    ///   - Default implementation throws
    ///   - Subclass can override if file support needed
    ///   - Caller can skip and use DeserializeFromBuffer instead
    ///
    virtual bool DeserializeFromFile(const std::string& filepath,
                                    ECS& ecsManager,
                                    const ComponentSchemaRegistry& registry) {
        (void)filepath;
        (void)ecsManager;
        (void)registry;
        throw std::runtime_error("DeserializeFromFile not implemented");
    }

    // ========================================================================
    // DIAGNOSTIC INTERFACE - Query deserialization results
    // ========================================================================

    /// Get the last error message
    ///
    /// RETURNS:
    ///   - String describing the most recent error
    ///   - Empty string if no error
    ///   - Never nullptr
    ///
    virtual std::string GetLastError() const = 0;

    /// Get number of bytes read
    ///
    /// RETURNS:
    ///   - Total bytes read in last deserialization
    ///   - 0 if no deserialization yet or failed
    ///
    virtual size_t GetBytesRead() const = 0;

    /// Check if last deserialization was successful
    ///
    /// RETURNS:
    ///   - true: Deserialization completed successfully
    ///   - false: Deserialization failed
    ///
    virtual bool WasSuccessful() const = 0;

    // ========================================================================
    // OPTIONAL INTERFACE - Additional diagnostics
    // ========================================================================

    /// Get deserializer name/format
    ///
    /// RETURNS:
    ///   - String identifying deserializer type
    ///   - Examples: "Normal", "Compressed", "JSON"
    ///
    virtual std::string GetDeserializerName() const {
        return "IDeserializer";
    }

    /// Get format version this deserializer handles
    ///
    /// RETURNS:
    ///   - Version number of format
    ///
    virtual uint32_t GetFormatVersion() const {
        return 1;
    }

    /// Check compatibility with format version
    ///
    /// INPUT:
    ///   - requiredVersion: Minimum version needed
    ///
    /// RETURNS:
    ///   - true: Can deserialize this version
    ///   - false: Version not supported
    ///
    virtual bool IsCompatible(uint32_t requiredVersion) const {
        return GetFormatVersion() >= requiredVersion;
    }

    // ========================================================================
    // STATISTICS - Deserialization progress
    // ========================================================================

    /// Get number of entities deserialized
    ///
    /// RETURNS:
    ///   - Count of entities successfully loaded
    ///   - 0 if not yet deserialized
    ///
    virtual uint32_t GetEntitiesDeserialized() const {
        return 0;  // Default, subclass overrides
    }

    /// Get number of components deserialized
    ///
    /// RETURNS:
    ///   - Total component count across all entities
    ///   - 0 if not yet deserialized
    ///
    virtual uint32_t GetComponentsDeserialized() const {
        return 0;  // Default, subclass overrides
    }

    /// Get number of fields deserialized
    ///
    /// RETURNS:
    ///   - Total field count across all components
    ///   - 0 if not yet deserialized
    ///
    virtual uint32_t GetFieldsDeserialized() const {
        return 0;  // Default, subclass overrides
    }
};
