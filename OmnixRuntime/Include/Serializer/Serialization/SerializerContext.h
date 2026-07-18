#pragma once

#include <cstdint>
#include <string>

// ============================================================================
// SERIALIZER CONTEXT - Configure serializer behavior without branching
// ============================================================================
//
// PURPOSE:
//   Control HOW serializers work without forcing internal branching logic
//   Pass context to serializer so it knows:
//     - What format to output (Text, Binary)
//     - What byte order to use (Little-endian, Big-endian)
//     - How to handle versioning (Strict, Lenient, Auto-migrate)
//     - How strict validation should be (Off, Minimal, Full)
//
// DESIGN PATTERN:
//   Caller creates context with desired behavior
//   Passes to serializer
//   Serializer uses context to customize output
//   No branching in serializer code ("if JSON then... else if Binary then...")
//
// BENEFIT:
//   - Easy to add new formats without modifying serializer
//   - Same serializer works with different configurations
//   - Context is testable independently
//   - Clear configuration object (not scattered parameters)
//
// ============================================================================

// ============================================================================
// ENUMERATIONS - Configuration options
// ============================================================================

/// Output format/layout type
enum class SerializerLayout : uint8_t {
    BINARY  = 0,  // Compact binary format (fastest, smallest)
    TEXT    = 1,  // Human-readable text format (JSON/XML style)
    JSON    = 2,  // Explicit JSON format
    XML     = 3   // Explicit XML format
};

/// Byte order for multi-byte values
enum class Endianness : uint8_t {
    NATIVE       = 0,  // System native (fast, not portable)
    LITTLE_ENDIAN = 1, // Intel x86/x64 standard (most common)
    BIG_ENDIAN   = 2   // PowerPC/network standard
};

/// How to handle version mismatches
enum class VersionPolicy : uint8_t {
    STRICT = 0,        // Fail if versions don't match exactly
    LENIENT = 1,       // Accept if major version matches
    AUTO_MIGRATE = 2   // Try to auto-convert between versions
};

/// Validation strictness level
enum class ValidationLevel : uint8_t {
    NONE = 0,       // No validation (fastest, least safe)
    MINIMAL = 1,    // Basic checks only (magic number, entity count)
    FULL = 2        // Complete validation (every field, every value)
};

// ============================================================================
// CLASS: SerializerContext - Configuration container
// ============================================================================

class SerializerContext {
private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================
    
    SerializerLayout m_Layout;              // What format to produce
    Endianness m_Endianness;                // Byte order
    VersionPolicy m_VersionPolicy;          // How to handle versions
    ValidationLevel m_ValidationLevel;      // Validation strictness
    
    uint32_t m_MinimumVersion;              // Minimum acceptable version
    bool m_IncludeMetadata;                 // Write extra metadata?
    bool m_WriteChecksum;                   // Calculate and write checksum?
    bool m_Compressed;                      // Apply compression?
    
public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    
    /// Create serializer context with defaults
    ///
    /// DEFAULT CONFIGURATION:
    ///   - Layout: BINARY (fastest)
    ///   - Endianness: LITTLE_ENDIAN (most common)
    ///   - VersionPolicy: STRICT (safe)
    ///   - ValidationLevel: FULL (safest)
    ///   - MinimumVersion: 1
    ///   - IncludeMetadata: true
    ///   - WriteChecksum: true
    ///   - Compressed: false
    ///
    explicit SerializerContext(SerializerLayout layout = SerializerLayout::BINARY)
        : m_Layout(layout),
          m_Endianness(Endianness::LITTLE_ENDIAN),
          m_VersionPolicy(VersionPolicy::STRICT),
          m_ValidationLevel(ValidationLevel::FULL),
          m_MinimumVersion(1),
          m_IncludeMetadata(true),
          m_WriteChecksum(true),
          m_Compressed(false) {
    }
    
    // ========================================================================
    // LAYOUT CONFIGURATION - What format?
    // ========================================================================
    
    /// Set output format/layout
    ///
    /// INPUT:
    ///   - layout: SerializerLayout enum value
    ///
    /// EXAMPLES:
    ///   context.SetLayout(SerializerLayout::BINARY);
    ///   context.SetLayout(SerializerLayout::JSON);
    ///   context.SetLayout(SerializerLayout::XML);
    ///
    void SetLayout(SerializerLayout layout) {
        m_Layout = layout;
    }
    
    SerializerLayout GetLayout() const {
        return m_Layout;
    }
    
    bool IsBinaryLayout() const {
        return m_Layout == SerializerLayout::BINARY;
    }
    
    bool IsTextLayout() const {
        return m_Layout == SerializerLayout::TEXT || 
               m_Layout == SerializerLayout::JSON || 
               m_Layout == SerializerLayout::XML;
    }
    
    bool IsJsonLayout() const {
        return m_Layout == SerializerLayout::JSON;
    }
    
    bool IsXmlLayout() const {
        return m_Layout == SerializerLayout::XML;
    }
    
    const char* GetLayoutName() const {
        switch (m_Layout) {
            case SerializerLayout::BINARY: return "BINARY";
            case SerializerLayout::TEXT: return "TEXT";
            case SerializerLayout::JSON: return "JSON";
            case SerializerLayout::XML: return "XML";
            default: return "UNKNOWN";
        }
    }
    
    // ========================================================================
    // ENDIANNESS CONFIGURATION - What byte order?
    // ========================================================================
    
    /// Set byte order for multi-byte values
    ///
    /// IMPORTANT:
    ///   - NATIVE: Fast but not portable across systems
    ///   - LITTLE_ENDIAN: Standard on most modern systems (x86, ARM)
    ///   - BIG_ENDIAN: Network standard, older systems
    ///
    /// RECOMMENDATION:
    ///   - Use LITTLE_ENDIAN for saves (portable)
    ///   - Use NATIVE for temporary files (fastest)
    ///
    void SetEndianness(Endianness endian) {
        m_Endianness = endian;
    }
    
    Endianness GetEndianness() const {
        return m_Endianness;
    }
    
    bool IsNativeEndian() const {
        return m_Endianness == Endianness::NATIVE;
    }
    
    bool IsLittleEndian() const {
        return m_Endianness == Endianness::LITTLE_ENDIAN;
    }
    
    bool IsBigEndian() const {
        return m_Endianness == Endianness::BIG_ENDIAN;
    }
    
    const char* GetEndianName() const {
        switch (m_Endianness) {
            case Endianness::NATIVE: return "NATIVE";
            case Endianness::LITTLE_ENDIAN: return "LITTLE_ENDIAN";
            case Endianness::BIG_ENDIAN: return "BIG_ENDIAN";
            default: return "UNKNOWN";
        }
    }
    
    // ========================================================================
    // VERSION POLICY CONFIGURATION - How to handle versions?
    // ========================================================================
    
    /// Set version mismatch policy
    ///
    /// OPTIONS:
    ///   - STRICT: Must match exactly (safest, might reject valid saves)
    ///   - LENIENT: Major version must match (flexible)
    ///   - AUTO_MIGRATE: Try to convert between versions automatically
    ///
    /// USAGE:
    ///   - Save games: Use LENIENT or AUTO_MIGRATE
    ///   - Network: Use STRICT (both sides same version)
    ///   - Debug: Use STRICT (catch mismatches early)
    ///
    void SetVersionPolicy(VersionPolicy policy) {
        m_VersionPolicy = policy;
    }
    
    VersionPolicy GetVersionPolicy() const {
        return m_VersionPolicy;
    }
    
    void SetMinimumVersion(uint32_t version) {
        m_MinimumVersion = version;
    }
    
    uint32_t GetMinimumVersion() const {
        return m_MinimumVersion;
    }
    
    bool IsStrict() const {
        return m_VersionPolicy == VersionPolicy::STRICT;
    }
    
    bool IsLenient() const {
        return m_VersionPolicy == VersionPolicy::LENIENT;
    }
    
    bool CanAutoMigrate() const {
        return m_VersionPolicy == VersionPolicy::AUTO_MIGRATE;
    }
    
    const char* GetVersionPolicyName() const {
        switch (m_VersionPolicy) {
            case VersionPolicy::STRICT: return "STRICT";
            case VersionPolicy::LENIENT: return "LENIENT";
            case VersionPolicy::AUTO_MIGRATE: return "AUTO_MIGRATE";
            default: return "UNKNOWN";
        }
    }
    
    // ========================================================================
    // VALIDATION CONFIGURATION - How strict?
    // ========================================================================
    
    /// Set validation strictness level
    ///
    /// OPTIONS:
    ///   - NONE: Skip validation (fastest, least safe)
    ///   - MINIMAL: Check magic number, entity count (balance)
    ///   - FULL: Validate every field, every value (safest, slower)
    ///
    /// USAGE:
    ///   - Production: Use FULL or MINIMAL
    ///   - Network: Use MINIMAL (peer is trusted)
    ///   - Debug: Use FULL (catch errors early)
    ///
    void SetValidationLevel(ValidationLevel level) {
        m_ValidationLevel = level;
    }
    
    ValidationLevel GetValidationLevel() const {
        return m_ValidationLevel;
    }
    
    bool ShouldValidate() const {
        return m_ValidationLevel != ValidationLevel::NONE;
    }
    
    bool IsMinimalValidation() const {
        return m_ValidationLevel == ValidationLevel::MINIMAL;
    }
    
    bool IsFullValidation() const {
        return m_ValidationLevel == ValidationLevel::FULL;
    }
    
    const char* GetValidationLevelName() const {
        switch (m_ValidationLevel) {
            case ValidationLevel::NONE: return "NONE";
            case ValidationLevel::MINIMAL: return "MINIMAL";
            case ValidationLevel::FULL: return "FULL";
            default: return "UNKNOWN";
        }
    }
    
    // ========================================================================
    // ADDITIONAL OPTIONS
    // ========================================================================
    
    /// Include extra metadata (timestamps, checksums, etc.)
    void SetIncludeMetadata(bool include) {
        m_IncludeMetadata = include;
    }
    
    bool GetIncludeMetadata() const {
        return m_IncludeMetadata;
    }
    
    /// Write checksum for error detection
    void SetWriteChecksum(bool write) {
        m_WriteChecksum = write;
    }
    
    bool GetWriteChecksum() const {
        return m_WriteChecksum;
    }
    
    /// Apply compression to output
    void SetCompressed(bool compressed) {
        m_Compressed = compressed;
    }
    
    bool IsCompressed() const {
        return m_Compressed;
    }
    
    // ========================================================================
    // VALIDATION & DEBUG
    // ========================================================================
    
    /// Validate context is sensible
    ///
    /// CHECKS:
    ///   - MinimumVersion > 0
    ///   - Layout is valid
    ///   - Endianness is valid
    ///
    /// RETURNS:
    ///   - true: Context is valid
    ///   - false: Context has invalid values
    ///
    bool IsValid() const {
        if (m_MinimumVersion == 0) {
            return false;
        }
        
        if (m_Layout > SerializerLayout::XML) {
            return false;
        }
        
        if (m_Endianness > Endianness::BIG_ENDIAN) {
            return false;
        }
        
        return true;
    }
    
    /// Get human-readable description of context
    std::string GetDescription() const {
        std::string desc;
        
        desc += "SerializerContext {\n";
        desc += "  Layout: " + std::string(GetLayoutName()) + "\n";
        desc += "  Endianness: " + std::string(GetEndianName()) + "\n";
        desc += "  VersionPolicy: " + std::string(GetVersionPolicyName()) + "\n";
        desc += "  MinimumVersion: " + std::to_string(m_MinimumVersion) + "\n";
        desc += "  ValidationLevel: " + std::string(GetValidationLevelName()) + "\n";
        desc += "  IncludeMetadata: " + std::string(m_IncludeMetadata ? "yes" : "no") + "\n";
        desc += "  WriteChecksum: " + std::string(m_WriteChecksum ? "yes" : "no") + "\n";
        desc += "  Compressed: " + std::string(m_Compressed ? "yes" : "no") + "\n";
        desc += "}";
        
        return desc;
    }
};

// ============================================================================
// PRESET CONTEXTS - Common configurations
// ============================================================================

/// Create context for game save (safe, portable, complete)
inline SerializerContext CreateSaveContext() {
    SerializerContext ctx(SerializerLayout::BINARY);
    ctx.SetEndianness(Endianness::LITTLE_ENDIAN);  // Portable
    ctx.SetVersionPolicy(VersionPolicy::LENIENT);   // Accept minor updates
    ctx.SetValidationLevel(ValidationLevel::FULL);  // Catch corruption
    ctx.SetWriteChecksum(true);                     // Error detection
    ctx.SetIncludeMetadata(true);                   // Timestamps, etc.
    return ctx;
}

/// Create context for network transmission (fast, optimized)
inline SerializerContext CreateNetworkContext() {
    SerializerContext ctx(SerializerLayout::BINARY);
    ctx.SetEndianness(Endianness::LITTLE_ENDIAN);  // Assume same platform
    ctx.SetVersionPolicy(VersionPolicy::STRICT);    // Peers must match
    ctx.SetValidationLevel(ValidationLevel::MINIMAL); // Peer is trusted
    ctx.SetWriteChecksum(false);                    // Network has checksums
    ctx.SetIncludeMetadata(false);                  // Minimal overhead
    return ctx;
}

/// Create context for debug output (readable, verbose)
inline SerializerContext CreateDebugContext() {
    SerializerContext ctx(SerializerLayout::JSON);
    ctx.SetEndianness(Endianness::LITTLE_ENDIAN);
    ctx.SetVersionPolicy(VersionPolicy::STRICT);
    ctx.SetValidationLevel(ValidationLevel::FULL);
    ctx.SetWriteChecksum(true);
    ctx.SetIncludeMetadata(true);
    return ctx;
}

/// Create context for compressed storage (space-efficient)
inline SerializerContext CreateCompressedContext() {
    SerializerContext ctx(SerializerLayout::BINARY);
    ctx.SetEndianness(Endianness::LITTLE_ENDIAN);
    ctx.SetVersionPolicy(VersionPolicy::LENIENT);
    ctx.SetValidationLevel(ValidationLevel::MINIMAL);
    ctx.SetWriteChecksum(true);
    ctx.SetCompressed(true);
    return ctx;
}

/// Create context for maximum compatibility (permissive)
inline SerializerContext CreateCompatibilityContext() {
    SerializerContext ctx(SerializerLayout::BINARY);
    ctx.SetEndianness(Endianness::LITTLE_ENDIAN);
    ctx.SetVersionPolicy(VersionPolicy::AUTO_MIGRATE);  // Auto-convert
    ctx.SetValidationLevel(ValidationLevel::MINIMAL);    // Be lenient
    ctx.SetWriteChecksum(true);
    return ctx;
}
