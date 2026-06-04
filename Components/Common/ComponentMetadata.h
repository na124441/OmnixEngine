#ifndef OMX_COMPONENT_METADATA_H
#define OMX_COMPONENT_METADATA_H

#include <stddef.h>
#include <stdint.h>

/* Field types */
typedef enum
{
    FIELD_FLOAT,
    FIELD_INT,
    FIELD_UINT,
    FIELD_BOOL,
    FIELD_VEC2,
    FIELD_VEC3,
    FIELD_VEC4,
    FIELD_MAT4,
    FIELD_CUSTOM
} FieldType;

/* Component field descriptor */
typedef struct
{
    const char* name;   // field name
    FieldType type;     // field type
    size_t offset;      // offsetof(struct, field)
    size_t size;        // sizeof(field)
} OmxComponentField;

/* Component schema descriptor */
typedef struct
{
    const char* name;               // component name
    uint32_t type_id;               // unique component ID
    size_t size;                    // sizeof(component)
    uint32_t field_count;           // number of fields
    const OmxComponentField* fields;// pointer to fields array
} OmxComponentSchema;

#endif // OMX_COMPONENT_METADATA_H
