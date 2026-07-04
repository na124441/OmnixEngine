#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Push Constant for the instance index
layout(push_constant) uniform PushConstants {
    uint instanceIndex;
} pc;

// GPUSceneBindings
#define GPUSCENE_BINDING_CAMERA 0
#define GPUSCENE_BINDING_INSTANCES 1
#define GPUSCENE_BINDING_MATERIALS 2
#define GPUSCENE_BINDING_LIGHTS 3

// Binding 0: Camera Uniform Buffer
layout(set = 0, binding = GPUSCENE_BINDING_CAMERA) uniform RadianceFrame
{
    mat4 view;
    mat4 projection;
    mat4 inverseView;
    mat4 inverseProjection;

    vec4 cameraPosition;
    vec4 viewportSize;

    vec4 skyTopColorIntensity;
    vec4 skyHorizonColorBlend;
    vec4 skyGroundColorIntensity;
    vec4 sunDirectionIntensity;
    vec4 sunColorAngularSize;

    vec4 exposureSettings;
    uvec4 renderFlags;
} frame;

struct InstanceData {
    mat4 worldMatrix;
    mat4 previousWorldMatrix;
    vec4 boundsCenterRadius;
    uint meshIndex;
    uint materialIndex;
    uint objectID;
    uint flags;
};

// Binding 1: Instance Storage Buffer
layout(std430, set = 0, binding = GPUSCENE_BINDING_INSTANCES) readonly buffer InstanceBuffer {
    InstanceData instances[];
} inst;

// G8 Bindings 8, 9, 10: RVG Metadata storage buffers
struct GPURVGNode {
    uint clusterId;
    float geometricError;
    uint parentNodeId;
    uint childCount;
    uint childNodeIds[4];
};

struct GPURVGCluster {
    vec4 boundsSphere;
    vec4 coneAxisCutoff;
    uint pageIndex;
    uint vertexOffset;
    uint indexOffset;
    uint vertexCount;
    uint indexCount;
    uint padding[3];
};

layout(std430, set = 0, binding = 8) readonly buffer RVGAssetBuffer
{
    uint dummy[];
} assetBuf;

layout(std430, set = 0, binding = 9) readonly buffer RVGNodeBuffer
{
    GPURVGNode nodes[];
} nodeBuf;

layout(std430, set = 0, binding = 10) readonly buffer RVGClusterBuffer
{
    GPURVGCluster clusters[];
} clusterBuf;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vCameraPos;
layout(location = 4) flat out uint vMaterialIndex;
layout(location = 5) flat out uint vEntityID;
layout(location = 6) out vec3 vDebugColor;

void main()
{
    uint rawInstanceIndex = pc.instanceIndex;
    uint instanceIndex = rawInstanceIndex;
    uint nodeIndex = 0;
    bool isVirtual = false;

    // Detect if we are drawing virtual geometry
    uint tempInstanceIndex = rawInstanceIndex & 0xFFF;
    InstanceData tempInst = inst.instances[tempInstanceIndex];
    if ((tempInst.flags & 4) != 0) {
        instanceIndex = rawInstanceIndex & 0xFFF;
        nodeIndex = rawInstanceIndex >> 12;
        isVirtual = true;
    }

    mat4 worldMatrix = inst.instances[instanceIndex].worldMatrix;

    vec4 worldPos = worldMatrix * vec4(inPos, 1.0);
    vWorldPos = worldPos.xyz;

    mat3 normalMat = transpose(inverse(mat3(worldMatrix)));
    vNormal = normalize(normalMat * inNormal);

    vUV = inUV;
    vCameraPos = frame.cameraPosition.xyz;
    vMaterialIndex = inst.instances[instanceIndex].materialIndex;
    vEntityID = inst.instances[instanceIndex].objectID;

    // Debug Color Calculation
    vDebugColor = vec3(0.0);
    uint debugMode = frame.renderFlags.w & 0xFF;
    if (isVirtual && debugMode != 0) {
        if (debugMode == 1) {
            // Hierarchy Level
            uint depth = 0;
            uint currNode = nodeIndex;
            while (currNode != 0xFFFFFFFF && depth < 16) {
                currNode = nodeBuf.nodes[currNode].parentNodeId;
                depth++;
            }
            vec3 depthColors[6] = vec3[](
                vec3(1.0, 0.0, 0.0), // Red
                vec3(0.0, 1.0, 0.0), // Green
                vec3(0.0, 0.0, 1.0), // Blue
                vec3(1.0, 1.0, 0.0), // Yellow
                vec3(1.0, 0.0, 1.0), // Magenta
                vec3(0.0, 1.0, 1.0)  // Cyan
            );
            vDebugColor = depthColors[depth % 6];
        }
        else if (debugMode == 2) {
            // Geometric Error
            float error = nodeBuf.nodes[nodeIndex].geometricError;
            float t = clamp(error * 10.0, 0.0, 1.0);
            vDebugColor = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), t);
        }
        else if (debugMode == 3) {
            // Projected Error
            GPURVGCluster cluster = clusterBuf.clusters[nodeBuf.nodes[nodeIndex].clusterId];
            vec4 sphereLocal = cluster.boundsSphere;
            vec3 centerWS = (worldMatrix * vec4(sphereLocal.xyz, 1.0)).xyz;
            float scaleX = length(worldMatrix[0].xyz);
            float scaleY = length(worldMatrix[1].xyz);
            float scaleZ = length(worldMatrix[2].xyz);
            float maxScale = max(scaleX, max(scaleY, scaleZ));

            vec4 viewCenter = frame.view * vec4(centerWS, 1.0);
            float viewDepth = max(-viewCenter.z, 0.0001);
            float projectionScale = frame.viewportSize.y * frame.projection[1][1] * 0.5;
            float projectedError = nodeBuf.nodes[nodeIndex].geometricError * maxScale * projectionScale / viewDepth;

            float targetErr = 2.0;
            float t = clamp(projectedError / targetErr, 0.0, 1.0);
            vDebugColor = mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), t);
        }
        else if (debugMode == 4) {
            // Selected Nodes
            float r = fract(sin(float(nodeIndex) * 12.9898) * 43758.5453);
            float g = fract(sin(float(nodeIndex) * 78.233) * 43758.5453);
            float b = fract(sin(float(nodeIndex) * 45.164) * 43758.5453);
            vDebugColor = vec3(r, g, b);
        }
        else if (debugMode == 5) {
            // Parent/Child Boundaries
            vDebugColor = (nodeBuf.nodes[nodeIndex].childCount == 0) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        }
    }

    gl_Position = frame.projection * frame.view * worldPos;
}
