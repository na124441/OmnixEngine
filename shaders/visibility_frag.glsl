#version 450

layout(location = 0) flat in uint vInstanceID;
layout(location = 1) flat in uint vClusterID;
layout(location = 2) flat in uint vPrimitiveID;

layout(location = 0) out uint outInstanceID;
layout(location = 1) out uint outClusterID;
layout(location = 2) out uint outPrimitiveID;

void main()
{
    outInstanceID = vInstanceID;
    outClusterID = vClusterID;
    outPrimitiveID = vPrimitiveID;
}
