#ifndef IDENTITY_COMPONENT_H
#define IDENTITY_COMPONENT_H

#include "Common/ComponentBase.h"
#include "Common/ComponentTraits.h"
#include "Common/ComponentMetadata.h"
#include "Common/ComponentClass.h"
#include <stddef.h>
#include <stdint.h>

/*
*
* Schema;
*   Enable --> Bool
*   Inheritec --> Bool
*   DisableReason --> enum
*/

/* ============================================================
   1. COMPONENT DATA
============================================================ */

typedef uint64_t TimeStamp;
// UUID struct (16 bytes)
typedef struct {
    TimeStamp createdAt;
    TimeStamp destroyedAt;
}Duration;

typedef struct
{
    TimeStamp BirthTime;
    TimeStamp DeathTime;
    Duration MaxLifeTime;
    Duration ElapsedTime;
    uint16_t Flags;
} LifeTime;


/* ============================================================
   2. COMPONENT TRAITS
============================================================ */

static const OmxComponentTraits LifeTime_Traits = OMX_DEFAULT_TRAITS;


/* ============================================================
   3. COMPONENT METADATA
============================================================ */

static const OmxComponentField LifeTime_Fields[] =
{
    { "BirthTime",   FIELD_CUSTOM, offsetof(LifeTime, BirthTime),   sizeof(TimeStamp) },
    { "DeathTime", FIELD_UINT,   offsetof(LifeTime, DeathTime), sizeof(TimeStamp) * 8 },
    { "MaxLifeTime", FIELD_UINT,   offsetof(LifeTime, MaxLifeTime), sizeof(Duration) * 8 },
    { "ElapsedTime",       FIELD_CUSTOM, offsetof(LifeTime, ElapsedTime),       sizeof(Duration) * 100 },
};

static const OmxComponentSchema LifeTime_Schema =
{
    "LifeTime",                          // readable name
    2,                                   // unique type ID (change if needed)
    sizeof(LifeTime),                    // size per entity
    sizeof(LifeTime_Fields) / sizeof(OmxComponentField),
    LifeTime_Fields
};


/* ============================================================
   4. COMPONENT CLASS
============================================================ */

static const OmxComponentClass LifeTime_Class   =
{
    1,                   // type ID
    &LifeTime_Schema,    // pointer to schema
    &LifeTime_Schema     // pointer to traits
};

#endif // IDENTITY_COMPONENT_H
