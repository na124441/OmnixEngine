#ifndef OMX_COMPONENT_TRAITS_H
#define OMX_COMPONENT_TRAITS_H

#include "ComponentBase.h"
#include <stdint.h>

/* ============================================================
   Trait Flags
   These are engine behaviour policies, not data.
   They never change per instance — only per component type.
   ============================================================ */

typedef enum
{
    OMX_TRAIT_NONE = 0,

    OMX_TRAIT_SERIALIZE = 1 << 0,  /* saved to disk */
    OMX_TRAIT_NETWORKED = 1 << 1,  /* replicated */
    OMX_TRAIT_SINGLETON = 1 << 2,  /* only one in world */
    OMX_TRAIT_TRANSIENT = 1 << 3,  /* never saved */
    OMX_TRAIT_TAG = 1 << 4   /* zero-size component */

} OmxTraitFlags;


/* ============================================================
   Optional lifecycle callbacks
   (Most components won't use them — and that's good)
   ============================================================ */

typedef void (*OmxComponentCtor)(void* component);
typedef void (*OmxComponentDtor)(void* component);


/* ============================================================
   Trait Description
   Attached to a component type
   ============================================================ */

typedef struct
{
    uint32_t flags;

    OmxComponentCtor constructor;
    OmxComponentDtor destructor;

} OmxComponentTraits;


/* ============================================================
   Defaults
   Most components will just use this.
   ============================================================ */

#define OMX_DEFAULT_TRAITS \
{ \
    OMX_TRAIT_SERIALIZE, \
    0, \
    0 \
}

#endif
