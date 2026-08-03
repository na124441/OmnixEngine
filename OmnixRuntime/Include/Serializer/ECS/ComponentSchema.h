#pragma once

// ---------------------------------------------------------------------------
// Component schema definitions – pure data structures.
// ---------------------------------------------------------------------------

// Required includes for dependent types.
#include "Serializer/ECS/FieldDelta.h"                     // defines FieldType enum
#include "../../Core/Logger.h"            // LOG_INFO, LOG_ERROR, etc.
#include "../../Core/Timer.h"             // Timer class (GetDeltaTime example)

// Intent flag bit‑masks.
constexpr uint32_t INTENT_SERIALIZABLE = 1u << 0; // field should be persisted / networked
constexpr uint32_t INTENT_NETWORKED   = 1u << 1; // field is sent over the network
// (add more flags as needed)

// ---------------------------------------------------------------------------
// 1. Field schema – describes a single member of a component.
// ---------------------------------------------------------------------------
struct FieldSchema {
    const char* name;        // Human‑readable field name
    size_t offset;           // Byte offset inside the component struct
    FieldType type;          // Enum: INT, FLOAT, VEC3, BOOL, …
    uint32_t intentFlags;    // Bitmask of INTENT_* flags
    size_t size;             // Size in bytes of the field
};

// ---------------------------------------------------------------------------
// 2. Component schema – describes an entire component type.
// ---------------------------------------------------------------------------
struct ComponentSchema {
    const char* componentName; // e.g. "HealthComponent"
    size_t componentSize;      // sizeof(component)
    FieldSchema* fields;       // Array of field descriptors
    int fieldCount;            // Number of entries in the array
};

// ---------------------------------------------------------------------------
// Example component (reference only).
// ---------------------------------------------------------------------------
struct HealthComponentExample {
    float health;        // offset 0
    float maxHealth;     // offset 4
    bool isDead;         // offset 8
    char damageType[32]; // offset 9
};

// Forward declarations for serialization helpers – full implementations live elsewhere.
class Serializer;
class Deserializer;

// ---------------------------------------------------------------------------
// Registration function – defined in a .cpp file.
// ---------------------------------------------------------------------------
void RegisterHealthComponentSchema();
