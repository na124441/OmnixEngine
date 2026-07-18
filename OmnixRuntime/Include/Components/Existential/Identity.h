#ifndef IDENTITY_COMPONENT_H
#define IDENTITY_COMPONENT_H

#include "Common/ComponentBase.h"
#include "Common/ComponentTraits.h"
#include "Common/ComponentMetadata.h"
#include "Common/ComponentClass.h"
#include <stddef.h>
#include <stdint.h>

/*
* Identity Component
* Responsibilities:
*   1. Uniquely identify an entity across worlds
*   2. Support hierarchy and ownerships
*
* Schema:
*   Identity { GlobalID, ArchtypeID, InstanceID, Name, Flags }
*/

/* ============================================================
   1. COMPONENT DATA
============================================================ */

// UUID struct (16 bytes)
typedef struct {
    uint8_t bytes[16];
} UUID;

typedef struct
{
    UUID GlobalID;         // 16 bytes
    uint8_t ArchtypeID[8]; // 8 bytes
    uint8_t InstanceID[8]; // 8 bytes
    char Name[100];        // 100 bytes
    uint32_t Flags;        // 32-bit bitmask
} Identity;


/* ============================================================
   2. COMPONENT TRAITS
============================================================ */

static const OmxComponentTraits Identity_Traits = OMX_DEFAULT_TRAITS;


/* ============================================================
   3. COMPONENT METADATA
============================================================ */

static const OmxComponentField Identity_Fields[] =
{
    { "GlobalID",   FIELD_CUSTOM, offsetof(Identity, GlobalID),   sizeof(UUID) },
    { "ArchtypeID", FIELD_UINT,   offsetof(Identity, ArchtypeID), sizeof(uint8_t) * 8 },
    { "InstanceID", FIELD_UINT,   offsetof(Identity, InstanceID), sizeof(uint8_t) * 8 },
    { "Name",       FIELD_CUSTOM, offsetof(Identity, Name),       sizeof(char) * 100 },
    { "Flags",      FIELD_UINT,   offsetof(Identity, Flags),      sizeof(uint32_t) }
};

static const OmxComponentSchema Identity_Schema =
{
    "Identity",                          // readable name
    1,                                   // unique type ID (change if needed)
    sizeof(Identity),                    // size per entity
    sizeof(Identity_Fields) / sizeof(OmxComponentField),
    Identity_Fields
};


/* ============================================================
   4. COMPONENT CLASS
============================================================ */

static const OmxComponentClass Identity_Class =
{
    1,                   // type ID
    &Identity_Schema,    // pointer to schema
    &Identity_Traits     // pointer to traits
};

#endif // IDENTITY_COMPONENT_H
