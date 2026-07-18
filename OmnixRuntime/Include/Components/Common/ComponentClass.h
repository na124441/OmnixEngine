#ifndef OMX_COMPONENT_CLASS_H
#define OMX_COMPONENT_CLASS_H

#include "Components/Common/ComponentBase.h"
#include "Components/Common/ComponentMetadata.h"
#include "Components/Common/ComponentTraits.h"
#include <EmotionBlending.h>

/* ============================================================
   Component Class
   The engine's runtime knowledge of a component type
   ============================================================ */

typedef struct
{
    OmxComponentTypeID id;

    const OmxComponentSchema* schema;
    const OmxComponentTraits* traits;

} OmxComponentClass;


/* ============================================================
   Lookup helpers
   (systems use these instead of knowing component types)
   ============================================================ */

static inline const char* OmxComponent_GetName(const OmxComponentClass* c)
{
    return c->schema->name;
}

static inline uint32_t OmxComponent_GetSize(const OmxComponentClass* c)
{
    return c->schema->size;
}

static inline uint32_t OmxComponent_GetFieldCount(const OmxComponentClass* c)
{
    return c->schema->field_count;
}

#endif
