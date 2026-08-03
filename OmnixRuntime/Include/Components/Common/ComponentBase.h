#ifndef OMX_COMPONENT_BASE_H
#define OMX_COMPONENT_BASE_H

#include <stdint.h>

/* ============================================================
   Fundamental Engine IDs
   These must NEVER change size across the entire engine.
   Save files and networking depend on this.
   ============================================================ */

typedef uint32_t OmxEntityID;
typedef uint32_t OmxComponentTypeID;

/* invalid constants */
#define OMX_INVALID_ENTITY        ((OmxEntityID)0)
#define OMX_INVALID_COMPONENTTYPE ((OmxComponentTypeID)0)


/* ============================================================
   Component Instance Handle
   This does NOT store the component itself.
   It only tells where it lives in ECS storage.
   ============================================================ */

typedef struct
{
    OmxEntityID        owner;      /* which entity owns it */
    OmxComponentTypeID type;       /* what kind of component */
    uint32_t           index;      /* index inside pool/chunk */

} OmxComponentHandle;


/* invalid handle */
static const OmxComponentHandle OMX_INVALID_HANDLE = { 0,0,0 };


/* ============================================================
   Utility helpers (header-only, zero dependency)
   ============================================================ */

static inline int OmxHandle_IsValid(OmxComponentHandle h)
{
    return (h.owner != OMX_INVALID_ENTITY);
}

#endif
