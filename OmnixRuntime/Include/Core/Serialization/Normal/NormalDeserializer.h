// Core/Serialization/Normal/NormalDeserializer.h

#pragma once

#include "../IDeserializer.h"
#include <vector>
#include <cstdint>
#include <string>

// Forward declarations
class ECSSnapshot;
class EntitySnapshot;
class ComponentSnapshot;
class FieldSnapshot;
class SerializerContext;
class ECS;
class ComponentSchemaRegistry;

// ============================================================================
// CLASS: NormalDeserializer - Read binary snapshot into ECSSnapshot
// ============================================================================
//
// DESIGN:
//   Two-phase deserialization:
//   Phase 1: Read binary → ECSSnapshot (staging/validation)
//   Phase 2: Apply ECSSnapshot → ECS (instantiation)
//
// WHY TWO PHASES:
//   ✓ Validation happens before ECS modification
//   ✓ Can abort without corrupting ECS
//   ✓ ECSSnapshot can be inspected, logged, copied
//   ✓ Easier to implement and test
//
// ALGORITHM (as per outline):
//   1. Initialize empty ECSSnapshot
//   2. Read & validate header
//   3. Read entity count
//   4. For each entity:
//      - Read entity metadata
//      - For each component:
//        - Read component metadata
//        - For each field:
//          - Read field data
//   5. Finalize & validate snapshot
//   6. Return completed snapshot
//
// ============================================================================

class NormalDeserializer : public IDeserializer {
private:
    // ========================================================================
    // PRIVATE MEMBERS - Deserialization state
    // ========================================================================
    
    // Input stream
    const uint8_t* m_Data;              // Binary buffer being read
    size_t m_Size;                      // Total buffer size
    size_t m_Position;                  // Current read position
    
    // Output state
    ECSSnapshot* m_CurrentSnapshot;     // Snapshot being built
    bool m_OwnsSnapshot;                // Do we own the memory?
    
    // Error tracking
    std::string m_LastError;
    bool m_IsValid;
    
    // Statistics
    uint32_t m_EntitiesDeserialized;
    uint32_t m_ComponentsDeserialized;
    uint32_t m_FieldsDeserialized;
    
    // ========================================================================
    // PRIVATE HELPER METHODS - Low-level reading
    // ========================================================================
    
    /// Check if we can read N bytes
    bool CanRead(size_t bytes) const;
    
    /// Advance read position
    void Advance(size_t bytes);
    
    /// Read primitive types
    bool ReadUInt8(uint8_t& value);
    bool ReadUInt32(uint32_t& value);
    bool ReadUInt64(uint64_t& value);
    bool ReadFloat(float& value);
    bool ReadBytes(uint8_t* buffer, size_t size);
    bool ReadString(std::string& str);
    
    // ========================================================================
    // PRIVATE HELPER METHODS - Validation
    // ========================================================================
    
    /// Validate header magic number
    bool ValidateMagicNumber(uint32_t magic);
    
    /// Validate format version
    bool ValidateVersion(uint32_t version);
    
    /// Validate entity count (not too many)
    bool ValidateEntityCount(uint32_t count);
    
    /// Validate component count per entity
    bool ValidateComponentCount(uint32_t count);
    
    /// Validate field count per component
    bool ValidateFieldCount(uint32_t count);
    
    /// Validate field size
    bool ValidateFieldSize(uint32_t size);
    
    // ========================================================================
    // PRIVATE HELPER METHODS - 6-step pipeline
    // ========================================================================
    
    /// STEP 1: Initialize empty snapshot
    bool InitializeSnapshot();
    
    /// STEP 2: Read and validate header
    ///   - Read magic number
    ///   - Read versions
    ///   - Read timestamp
    ///   - Validate all values
    ///   - Store in snapshot header
    bool ReadAndValidateHeader();
    
    /// STEP 3: Read entity count
    ///   - Read count from buffer
    ///   - Validate count
    ///   - Reserve space in snapshot
    bool ReadEntityCount(uint32_t& outCount);
    
    /// STEP 4a: Read entity metadata
    ///   - Read entity ID
    ///   - Read entity generation
    ///   - Read component count
    bool BeginEntity(uint32_t& outEntityID, 
                    uint32_t& outGeneration,
                    uint32_t& outComponentCount);
    
    /// STEP 4b: Read component metadata
    ///   - Read component type ID
    ///   - Read component version
    ///   - Read field count
    bool BeginComponent(uint32_t& outComponentTypeID,
                       uint32_t& outComponentVersion,
                       uint32_t& outFieldCount);
    
    /// STEP 4c: Read field data
    ///   - Read field ID
    ///   - Read field type
    ///   - Read field size
    ///   - Read field raw data
    bool ReadField(uint32_t& outFieldID,
                  uint8_t& outFieldType,
                  uint32_t& outFieldSize,
                  std::vector<uint8_t>& outFieldData);
    
    /// STEP 4d: End entity processing
    ///   - Create EntitySnapshot with all components
    ///   - Append to snapshot
    bool EndEntity(const EntitySnapshot& entitySnapshot);
    
    /// STEP 5: Finalize and validate
    ///   - Check snapshot is complete
    ///   - Verify entity count matches
    ///   - Calculate checksum if needed
    bool FinalizeSnapshot();
    
    // ========================================================================
    // PRIVATE HELPER METHODS - Component/Entity building
    // ========================================================================
    
    /// Build entity snapshot from read data
    EntitySnapshot* BuildEntitySnapshot(uint32_t entityID,
                                       uint32_t generation);
    
    /// Build component snapshot from read data
    ComponentSnapshot* BuildComponentSnapshot(uint32_t typeID,
                                             uint32_t version);
    
    /// Add field to component
    bool AddFieldToComponent(ComponentSnapshot& component,
                            uint32_t fieldID,
                            uint8_t fieldType,
                            const std::vector<uint8_t>& fieldData);
    
public:
    // ========================================================================
    // CONSTRUCTION & DESTRUCTION
    // ========================================================================
    
    /// Create deserializer with optional context
    explicit NormalDeserializer(const SerializerContext* context = nullptr);
    
    /// Destructor - clean up snapshot if owned
    ~NormalDeserializer();
    
    // ========================================================================
    // PUBLIC INTERFACE - Implement IDeserializer
    // ========================================================================
    
    /// Main deserialization entry point
    /// Reads binary buffer, reconstructs into ECSSnapshot
    /// Then applies to live ECS
    bool Deserialize(const uint8_t* data,
                    size_t size,
                    ECS& ecsManager,
                    const ComponentSchemaRegistry& registry) override;
    
    /// Deserialize to ECSSnapshot only (no ECS modification)
    /// Useful for validation, inspection, offline processing
    ECSSnapshot* DeserializeToSnapshot(const uint8_t* data,
                                      size_t size,
                                      const ComponentSchemaRegistry& registry);
    
    // ========================================================================
    // DIAGNOSTIC INTERFACE - Implement IDeserializer
    // ========================================================================
    
    std::string GetLastError() const override;
    
    size_t GetBytesRead() const override;
    
    bool WasSuccessful() const override;
    
    std::string GetDeserializerName() const override;
    
    uint32_t GetFormatVersion() const override;
    
    // ========================================================================
    // STATISTICS - Deserialization progress
    // ========================================================================
    
    uint32_t GetEntitiesDeserialized() const override;
    
    uint32_t GetComponentsDeserialized() const override;
    
    uint32_t GetFieldsDeserialized() const override;
};
