#version 450
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inUV;

// Set 0 Binding 10: ObjectID texture for entity ID
layout(set = 0, binding = 10) uniform usampler2D objectIDTex;

layout(push_constant) uniform PushConstants
{
    uint selectedEntityID;
    float outlineThickness;
    vec4 outlineColor;
} pcs;

void main()
{
    uint selectedID = pcs.selectedEntityID;
    if (selectedID == 0)
    {
        discard;
    }

    // ObjectID contains raw 32-bit unsigned entity IDs
    uint centerID = texture(objectIDTex, inUV).r;

    vec2 texelSize = 1.0 / vec2(textureSize(objectIDTex, 0));
    
    // Check 4-neighbor pixels for outline
    uint lID = texture(objectIDTex, inUV + vec2(-texelSize.x * pcs.outlineThickness, 0.0)).r;
    uint rID = texture(objectIDTex, inUV + vec2(texelSize.x * pcs.outlineThickness, 0.0)).r;
    uint uID = texture(objectIDTex, inUV + vec2(0.0, -texelSize.y * pcs.outlineThickness)).r;
    uint dID = texture(objectIDTex, inUV + vec2(0.0, texelSize.y * pcs.outlineThickness)).r;
    
    bool isEdge = false;
    if (centerID == selectedID)
    {
        if (lID != selectedID || rID != selectedID || uID != selectedID || dID != selectedID)
        {
            isEdge = true;
        }
    }
    else
    {
        if (lID == selectedID || rID == selectedID || uID == selectedID || dID == selectedID)
        {
            isEdge = true;
        }
    }
    
    if (isEdge)
    {
        outColor = pcs.outlineColor;
    }
    else
    {
        discard;
    }
}
