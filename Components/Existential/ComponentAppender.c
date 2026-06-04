#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 50
#define MAX_NAME_LEN 64
#define MAX_TYPE_LEN 32
#define TEMPLATE_BUFFER 8192

typedef struct {
    char name[MAX_NAME_LEN];
    char type[MAX_TYPE_LEN];
    int array_size; // 0 means not an array
} Field;

// Helper: write field line for struct
void generate_struct_field(char *buffer, Field f) {
    if(f.array_size > 0)
        sprintf(buffer + strlen(buffer), "    %s %s[%d];\n", f.type, f.name, f.array_size);
    else
        sprintf(buffer + strlen(buffer), "    %s %s;\n", f.type, f.name);
}

// Helper: write field line for metadata
void generate_field_descriptor(char *buffer, char *component_name, Field f, int last) {
    char field_type_str[32];
    if(strcmp(f.type, "float") == 0) strcpy(field_type_str, "FIELD_FLOAT");
    else if(strcmp(f.type, "int") == 0 || strcmp(f.type, "uint32_t") == 0) strcpy(field_type_str, "FIELD_UINT");
    else strcpy(field_type_str, "FIELD_CUSTOM");

    if(f.array_size > 0)
        sprintf(buffer + strlen(buffer),
            "    { \"%s\", %s, offsetof(%s, %s), sizeof(%s) * %d }%s\n",
            f.name, field_type_str, component_name, f.name, f.type, f.array_size, last ? "" : ",");
    else
        sprintf(buffer + strlen(buffer),
            "    { \"%s\", %s, offsetof(%s, %s), sizeof(%s) }%s\n",
            f.name, field_type_str, component_name, f.name, f.type, last ? "" : ",");
}

int main() {
    char component_name[MAX_NAME_LEN];
    int num_fields;
    Field fields[MAX_FIELDS];

    printf("==== ECS Component Header Generator ====\n");
    printf("Enter component name: ");
    scanf("%s", component_name);

    printf("Enter number of fields: ");
    scanf("%d", &num_fields);
    getchar(); // consume newline

    for(int i=0;i<num_fields;i++) {
        printf("Field %d name: ", i+1);
        scanf("%s", fields[i].name);
        printf("Field %d type (float, int, UUID, etc.): ", i+1);
        scanf("%s", fields[i].type);
        printf("Field %d array size (0 if not array): ", i+1);
        scanf("%d", &fields[i].array_size);
        getchar(); // consume newline
    }

    // Start generating
    char struct_fields[1024] = "";
    char field_descriptors[2048] = "";

    for(int i=0;i<num_fields;i++) {
        generate_struct_field(struct_fields, fields[i]);
        generate_field_descriptor(field_descriptors, component_name, fields[i], i==num_fields-1);
    }

    // Template
    char output[TEMPLATE_BUFFER];
    snprintf(output, TEMPLATE_BUFFER,
"#ifndef %s_H\n"
"#define %s_H\n\n"
"#include \"Common/ComponentBase.h\"\n"
"#include \"Common/ComponentTraits.h\"\n"
"#include \"Common/ComponentMetadata.h\"\n"
"#include \"Common/ComponentClass.h\"\n"
"#include <stddef.h>\n"
"#include <stdint.h>\n\n"
"typedef struct {\n%s} %s;\n\n"
"static const OmxComponentTraits %s_Traits = OMX_DEFAULT_TRAITS;\n\n"
"static const OmxComponentField %s_Fields[] = {\n%s};\n\n"
"static const OmxComponentSchema %s_Schema = {\n"
"    \"%s\",\n"
"    1,\n"
"    sizeof(%s),\n"
"    sizeof(%s_Fields)/sizeof(OmxComponentField),\n"
"    %s_Fields\n"
"};\n\n"
"static const OmxComponentClass %s_Class = {\n"
"    1,\n"
"    &%s_Schema,\n"
"    &%s_Traits\n"
"};\n\n"
"#endif // %s_H\n",
component_name, component_name,
struct_fields, component_name,
component_name, component_name,
field_descriptors,
component_name, component_name, component_name, component_name,
component_name, component_name, component_name,
component_name
);

    // Write to file
    char filename[128];
    snprintf(filename, 128, "%s.h", component_name);
    FILE *f = fopen(filename, "w");
    if(!f) {
        printf("Error creating file %s\n", filename);
        return 1;
    }
    fputs(output, f);
    fclose(f);

    printf("Component header generated: %s\n", filename);
    return 0;
}
