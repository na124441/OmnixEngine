#pragma once

#include <stdexcept>
#include <vector>
#include <cstdint>
#include <memory>

// Forward declarations
class ECSSnapshot;

// ============================================================================
// INTERFACE: ISerializer - Abstract base for all snapshot serializers
// ============================================================================
// 
// CONTRACT: Any serializer MUST:
//   1. Accept a const ECSSnapshot&
//   2. Emit data in deterministic order (same data = same bytes every time)
//   3. Never open/close files directly (caller's responsibility)
//   4. Report errors clearly via GetLastError()
//   5. Track bytes written via GetBytesWritten()
//
// DESIGN PRINCIPLE: 
//   Serializers are responsible for DATA TRANSFORMATION ONLY
//   NOT for file I/O or stream management
//
// PATTERN:
//   Caller provides output destination (file, buffer, network, etc.)
//   Serializer writes to that destination
//   Caller manages lifetime and cleanup
//
// ============================================================================

class ISerializer {
public:
    // ========================================================================
    // VIRTUAL DESTRUCTOR (required for polymorphism)
    // ========================================================================
    
    /// Virtual destructor ensures proper cleanup of derived classes
    /// Must be virtual for polymorphic delete
    virtual ~ISerializer() = default;
    
    // ========================================================================
    // PRIMARY INTERFACE - All serializers must implement these
    // ========================================================================
    
    /// Serialize ECSSnapshot to binary format (abstract)
    /// 
    /// INPUT:
    ///   - snapshot: Const reference to ECSSnapshot to serialize
    ///   - Must be valid (IsValid() == true)
    ///
    /// PROCESS:
    ///   1. Validate snapshot
    ///   2. Write header (metadata)
    ///   3. Write entities (deterministically sorted)
    ///   4. Write components (for each entity)
    ///   5. Write fields (for each component)
    ///   6. Finalize (flush, checksum, etc.)
    ///
    /// OUTPUT:
    ///   - Binary data written to internal buffer
    ///   - Or to caller-provided output stream
    ///
    /// RETURNS:
    ///   - true: Serialization successful, data ready
    ///   - false: Serialization failed, check GetLastError()
    ///
    /// REQUIREMENTS:
    ///   - Must be deterministic (same input = same output bytes)
    ///   - Must write in same order every time
    ///   - Must not skip entities/components/fields
    ///   - Must validate snapshot before writing
    ///
    virtual bool Serialize(const ECSSnapshot& snapshot) = 0;
    
    // ========================================================================
    // OUTPUT VARIANTS - Different output destinations
    // ========================================================================
    
    /// Serialize directly to byte buffer (in-memory)
    ///
    /// PROCESS:
    ///   1. Call Serialize(snapshot)
    ///   2. Copy internal buffer to outBuffer
    ///
    /// OUTPUT:
    ///   - outBuffer: Vector filled with serialized bytes
    ///   - Automatically resized to fit data
    ///   - Cleared on failure
    ///
    /// RETURNS:
    ///   - true: Data written to outBuffer
    ///   - false: Failed, check GetLastError()
    ///
    /// NOTE:
    ///   - Useful for network transmission
    ///   - Or temporary storage before writing to file
    ///   - Caller manages buffer lifetime
    ///
    virtual bool SerializeToBuffer(const ECSSnapshot& snapshot,
                                  std::vector<uint8_t>& outBuffer) = 0;
    
    /// Serialize to file (caller provides path, not the serializer)
    ///
    /// IMPORTANT:
    ///   - This is a CONVENIENCE method
    ///   - Default implementation throws
    ///   - Subclass can override if file I/O is supported
    ///   - OR caller can skip and use SerializeToBuffer instead
    ///
    /// PROCESS:
    ///   1. Caller provides valid filepath
    ///   2. Serializer writes (implementation-specific)
    ///   3. Caller is responsible for file lifecycle
    ///
    /// RETURNS:
    ///   - true: Serialization successful
    ///   - false: Failed or not implemented
    ///
    /// NOTE:       
    ///   - Some serializers might not support files
    ///   - Default: throw std::runtime_error("Not implemented")
    ///   - Subclass can implement if needed
    ///
    virtual bool SerializeToFile(const ECSSnapshot& snapshot,
                                const std::string& filepath) {
        (void)snapshot;   // Avoid unused parameter warning
        (void)filepath;
        throw std::runtime_error("SerializeToFile not implemented by this serializer");
    }
    
    // ========================================================================
    // DIAGNOSTIC INTERFACE - Query serialization results
    // ========================================================================
    
    /// Get the last error message
    ///
    /// RETURNS:
    ///   - String describing the most recent error
    ///   - Empty string if no error
    ///   - Never nullptr
    ///
    /// USAGE:
    ///   if (!serializer->Serialize(snapshot)) {
    ///       std::cerr << "Error: " << serializer->GetLastError() << std::endl;
    ///   }
    ///
    virtual std::string GetLastError() const = 0;
    
    /// Get number of bytes written
    ///
    /// RETURNS:
    ///   - Total bytes written in last serialization
    ///   - 0 if no serialization yet or failed
    ///
    /// USAGE:
    ///   serializer->Serialize(snapshot);
    ///   std::cout << "Wrote " << serializer->GetBytesWritten() << " bytes" << std::endl;
    ///
    virtual size_t GetBytesWritten() const = 0;
    
    /// Check if last serialization was successful
    ///
    /// RETURNS:
    ///   - true: Serialization completed successfully
    ///   - false: Serialization failed or not yet called
    ///
    /// USAGE:
    ///   if (serializer->WasSuccessful()) {
    ///       // Use the serialized data
    ///   }
    ///
    virtual bool WasSuccessful() const = 0;
    
    // ========================================================================
    // OPTIONAL INTERFACE - For advanced use cases (default implementations)
    // ========================================================================
    
    /// Get serializer name/format
    ///
    /// RETURNS:
    ///   - String identifying serializer type
    ///   - Examples: "Normal", "Compressed", "Binary", "JSON"
    ///
    /// DEFAULT IMPLEMENTATION:
    ///   - Subclasses can override
    ///   - Used for logging/debugging
    ///
    virtual std::string GetSerializerName() const {
        return "ISerializer";
    }
    
    /// Get serializer format version
    ///
    /// RETURNS:
    ///   - Version number of serialization format
    ///   - Used to detect incompatible serializers
    ///   - Examples: 1, 2, 3 (increment when format changes)
    ///
    /// DEFAULT IMPLEMENTATION:
    ///   - Subclasses should override
    ///   - Default: version 1
    ///
    virtual uint32_t GetFormatVersion() const {
        return 1;
    }
    
    /// Validate that a serializer is compatible
    ///
    /// INPUT:
    ///   - requiredVersion: Minimum format version needed
    ///
    /// RETURNS:
    ///   - true: This serializer version meets requirement
    ///   - false: Incompatible
    ///
    /// DEFAULT IMPLEMENTATION:
    ///   - Checks: GetFormatVersion() >= requiredVersion
    ///
    virtual bool IsCompatible(uint32_t requiredVersion) const {
        return GetFormatVersion() >= requiredVersion;
    }
};

// ============================================================================
// USAGE PATTERNS
// ============================================================================

/*
PATTERN 1: Basic Serialization to Buffer
──────────────────────────────────────────

std::unique_ptr<ISerializer> serializer = 
    std::make_unique<NormalSerializer>();

std::vector<uint8_t> data;
if (serializer->SerializeToBuffer(snapshot, data)) {
    // data now contains serialized bytes
    // Use for network, disk, memory, etc.
    std::cout << "Serialized " << data.size() << " bytes" << std::endl;
} else {
    std::cerr << "Error: " << serializer->GetLastError() << std::endl;
}


PATTERN 2: Serialization to File
────────────────────────────────

std::unique_ptr<ISerializer> serializer = 
    std::make_unique<NormalSerializer>();

if (serializer->SerializeToFile(snapshot, "game.save")) {
    std::cout << "Saved " << serializer->GetBytesWritten() << " bytes" << std::endl;
} else {
    std::cerr << "Save failed: " << serializer->GetLastError() << std::endl;
}


PATTERN 3: Polymorphic Serializer Selection
───────────────────────────────────────────

std::unique_ptr<ISerializer> GetSerializerForContext(SnapshotType type) {
    switch (type) {
        case SNAPSHOT_SAVE:
            return std::make_unique<NormalSerializer>();
        case SNAPSHOT_NETWORK:
            return std::make_unique<CompressedSerializer>();
        case SNAPSHOT_DELTA:
            return std::make_unique<DeltaSerializer>();
        default:
            return nullptr;
    }
}

// Usage:
auto serializer = GetSerializerForContext(SNAPSHOT_NETWORK);
if (serializer && serializer->Serialize(snapshot)) {
    std::cout << "Format: " << serializer->GetSerializerName() << std::endl;
    SendDataToNetwork(serializer->GetBytesWritten());
}


PATTERN 4: Checking Compatibility
──────────────────────────────────

std::unique_ptr<ISerializer> serializer = 
    std::make_unique<NormalSerializer>();

uint32_t requiredVersion = 2;

if (serializer->IsCompatible(requiredVersion)) {
    // Serializer is version 2 or higher
    std::cout << "Serializer is compatible" << std::endl;
} else {
    std::cerr << "Serializer format too old!" << std::endl;
    std::cerr << "Have: " << serializer->GetFormatVersion() << std::endl;
    std::cerr << "Need: " << requiredVersion << std::endl;
}


PATTERN 5: Batch Operations (Compare Different Formats)
───────────────────────────────────────────────────────

std::vector<std::unique_ptr<ISerializer>> serializers = {
    std::make_unique<NormalSerializer>(),
    std::make_unique<CompressedSerializer>(),
    std::make_unique<JsonSerializer>()
};

for (auto& serializer : serializers) {
    std::vector<uint8_t> data;
    
    if (serializer->SerializeToBuffer(snapshot, data)) {
        std::cout << serializer->GetSerializerName() 
                  << ": " << data.size() << " bytes" << std::endl;
    } else {
        std::cerr << serializer->GetSerializerName() 
                  << " failed: " << serializer->GetLastError() << std::endl;
    }
}


PATTERN 6: Try Multiple Serializers
───────────────────────────────────

std::vector<std::unique_ptr<ISerializer>> serializers;
serializers.push_back(std::make_unique<NormalSerializer>());
serializers.push_back(std::make_unique<CompressedSerializer>());

std::vector<uint8_t> bestData;
std::string bestFormat;

for (auto& serializer : serializers) {
    std::vector<uint8_t> data;
    
    if (serializer->SerializeToBuffer(snapshot, data)) {
        if (bestData.empty() || data.size() < bestData.size()) {
            bestData = data;
            bestFormat = serializer->GetSerializerName();
        }
    }
}

if (!bestData.empty()) {
    std::cout << "Best format: " << bestFormat 
              << " (" << bestData.size() << " bytes)" << std::endl;
}
*/

// ============================================================================
// DESIGN PRINCIPLES EXPLAINED
// ============================================================================

/*
PRINCIPLE 1: SINGLE RESPONSIBILITY
───────────────────────────────────
Serializers do DATA TRANSFORMATION only:
  ✓ Convert snapshot → bytes
  ✗ Don't open files
  ✗ Don't manage streams
  ✗ Don't compress data (that's a separate concern)
  ✗ Don't encrypt data (that's a separate concern)

Why? So serializers are simple, testable, and reusable.


PRINCIPLE 2: DETERMINISM
────────────────────────
Same snapshot MUST produce same bytes:
  ✓ Sorted entity iteration (same order)
  ✓ Sorted component iteration (same order)
  ✓ Fixed byte order (little-endian everywhere)
  ✓ Same field order always
  ✓ No randomization or timestamps in data

Why? 
  - Saves are reproducible
  - Networks can validate checksums
  - Replays work correctly
  - Debugging is deterministic


PRINCIPLE 3: CALLER MANAGES I/O
───────────────────────────────
Serializers don't own streams:
  
  Serializer responsibility:     Caller's responsibility:
  ├─ Validate snapshot           ├─ Open file
  ├─ Write to buffer             ├─ Pass destination to serializer
  ├─ Track bytes written         ├─ Write buffer to disk/network
  └─ Report errors              └─ Close file/stream

Why? 
  - Decouples serialization from I/O
  - Easy to test (no file system needed)
  - Reusable in different contexts (file, network, memory)
  - Caller decides error handling


PRINCIPLE 4: POLYMORPHISM
─────────────────────────
All serializers implement same interface:
  
  ISerializer (abstract base)
      ├─ NormalSerializer
      ├─ CompressedSerializer
      ├─ JsonSerializer
      ├─ NetworkSerializer
      └─ CustomSerializer

Why? 
  - Easy to swap implementations without changing caller code
  - New serializers can be added without modifying existing code
  - Decouples caller from concrete serializers


PRINCIPLE 5: CLEAR ERROR HANDLING
──────────────────────────────────
Errors are reported, not thrown:
  ✓ Return bool for success/failure
  ✓ GetLastError() provides details
  ✓ Never throw from Serialize()
  ✓ Caller can decide how to handle errors

Why? 
  - Caller can decide error strategy
  - No exceptions in critical paths
  - Easier to debug (clear error messages)
  - Works in environments without exceptions
*/

// ============================================================================
// EXPECTED SUBCLASS IMPLEMENTATIONS
// ============================================================================

/*
CONCRETE SERIALIZERS:

1. NormalSerializer
   └─ Plain binary format
   └─ Deterministic order
   └─ No compression
   └─ Fastest, good for reference

2. CompressedSerializer
   └─ Binary format + compression
   └─ Same structure as Normal but with zlib/brotli
   └─ Slower but smaller files
   └─ Good for disk saves

3. JsonSerializer
   └─ Human-readable JSON format
   └─ Good for debugging/editing
   └─ Larger files, slower
   └─ Not for production

4. NetworkSerializer
   └─ Optimized for network bandwidth
   └─ Only serializes networked fields
   └─ Fast, small packets
   └─ Good for multiplayer

5. DeltaSerializer
   └─ Only serialize changed fields
   └─ Incremental updates
   └─ Smallest size
   └─ Good for bandwidth-limited connections

ALL MUST:
  ✓ Inherit from ISerializer
  ✓ Implement Serialize()
  ✓ Implement SerializeToBuffer()
  ✓ Track errors (GetLastError)
  ✓ Track bytes written (GetBytesWritten)
  ✓ Report success (WasSuccessful)
  ✓ Return deterministic binary
*/
