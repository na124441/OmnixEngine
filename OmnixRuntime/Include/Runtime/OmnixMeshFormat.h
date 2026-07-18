#pragma once
#include "Runtime/FileHeader.h"
#include "Runtime/AssetHandle.h"
#include "Runtime/BinaryReader.h"
#include "Runtime/BinaryWriter.h"
#include <vector>
#include <string>

#pragma pack(push, 1)

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    bool operator==(const Vec3& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    bool operator==(const Vec2& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    bool operator==(const Vec4& other) const noexcept {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
};

struct BoundingBox {
    Vec3 min;
    Vec3 max;

    bool operator==(const BoundingBox& other) const noexcept {
        return min == other.min && max == other.max;
    }
};

struct BoundingSphere {
    Vec3 center;
    float radius = 0.0f;

    bool operator==(const BoundingSphere& other) const noexcept {
        return center == other.center && radius == other.radius;
    }
};

struct OmnixMeshHeader
{
    FileHeader file;

    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t vertexStride = 0;
    uint32_t submeshCount = 0;

    uint32_t hasSkeleton = 0;
    uint32_t materialSlotCount = 0;

    BoundingBox bounds;
    BoundingSphere sphere;
};

struct OmnixVertex
{
    Vec3 position;
    Vec3 normal;
    Vec4 tangent;
    Vec2 uv0;
    Vec2 uv1;

    bool operator==(const OmnixVertex& o) const noexcept {
        return position == o.position && normal == o.normal && tangent == o.tangent && uv0 == o.uv0 && uv1 == o.uv1;
    }
};

struct OmnixSkinnedVertex
{
    Vec3 position;
    Vec3 normal;
    Vec4 tangent;
    Vec2 uv0;

    uint32_t jointIndices[4] = {0};
    float jointWeights[4] = {0.0f};

    bool operator==(const OmnixSkinnedVertex& o) const noexcept {
        return position == o.position && normal == o.normal && tangent == o.tangent && uv0 == o.uv0 &&
               jointIndices[0] == o.jointIndices[0] && jointIndices[1] == o.jointIndices[1] &&
               jointIndices[2] == o.jointIndices[2] && jointIndices[3] == o.jointIndices[3] &&
               jointWeights[0] == o.jointWeights[0] && jointWeights[1] == o.jointWeights[1] &&
               jointWeights[2] == o.jointWeights[2] && jointWeights[3] == o.jointWeights[3];
    }
};

struct OmnixSubmesh
{
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;

    bool operator==(const OmnixSubmesh& o) const noexcept {
        return indexStart == o.indexStart && indexCount == o.indexCount && materialIndex == o.materialIndex;
    }
};

#pragma pack(pop)

constexpr char MAGIC_MESH[8] = {'O', 'M', 'X', 'M', 'E', 'S', 'H', '\0'};
constexpr uint32_t OMNIX_MESH_VERSION_MAJOR = 1;
constexpr uint32_t OMNIX_MESH_VERSION_MINOR = 0;

struct OmnixMesh
{
    OmnixMeshHeader header;
    std::vector<OmnixVertex> vertices;
    std::vector<OmnixSkinnedVertex> skinnedVertices; // Used if header.hasSkeleton is true
    std::vector<uint32_t> indices;
    std::vector<OmnixSubmesh> submeshes;
    std::vector<AssetHandle> materialSlots;
    std::string skeletonAssetPath;
};

inline bool SerializeMesh(const OmnixMesh& mesh, const std::string& filepath) {
    eng::runtime::BinaryWriter writer;
    writer.BeginFile(MAGIC_MESH, OMNIX_MESH_VERSION_MAJOR, OMNIX_MESH_VERSION_MINOR);

    // Header info except FileHeader which is handled inside BeginFile
    writer.WriteU32(mesh.header.vertexCount);
    writer.WriteU32(mesh.header.indexCount);
    writer.WriteU32(mesh.header.vertexStride);
    writer.WriteU32(mesh.header.submeshCount);
    writer.WriteU32(mesh.header.hasSkeleton);
    writer.WriteU32(mesh.header.materialSlotCount);

    writer.WriteBytes(reinterpret_cast<const uint8_t*>(&mesh.header.bounds), sizeof(BoundingBox));
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(&mesh.header.sphere), sizeof(BoundingSphere));

    // Vertices
    if (mesh.header.hasSkeleton) {
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(mesh.skinnedVertices.data()), mesh.skinnedVertices.size() * sizeof(OmnixSkinnedVertex));
    } else {
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(mesh.vertices.data()), mesh.vertices.size() * sizeof(OmnixVertex));
    }

    // Indices
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(mesh.indices.data()), mesh.indices.size() * sizeof(uint32_t));

    // Submeshes
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(mesh.submeshes.data()), mesh.submeshes.size() * sizeof(OmnixSubmesh));

    // Material slots
    for (const auto& slot : mesh.materialSlots) {
        writer.WriteU64(slot.value);
    }

    // Optional skeleton reference path
    writer.WriteString(mesh.skeletonAssetPath);

    return writer.SaveToFile(filepath);
}

inline bool DeserializeMesh(OmnixMesh& mesh, const std::string& filepath) {
    eng::runtime::BinaryReader reader;
    if (!reader.LoadFromFile(filepath)) {
        return false;
    }

    if (!reader.ValidateHeaderAndChecksum(MAGIC_MESH, OMNIX_MESH_VERSION_MAJOR, OMNIX_MESH_VERSION_MINOR)) {
        return false;
    }

    try {
        mesh.header.vertexCount = reader.ReadU32();
        mesh.header.indexCount = reader.ReadU32();
        mesh.header.vertexStride = reader.ReadU32();
        mesh.header.submeshCount = reader.ReadU32();
        mesh.header.hasSkeleton = reader.ReadU32();
        mesh.header.materialSlotCount = reader.ReadU32();

        reader.ReadBytes(reinterpret_cast<uint8_t*>(&mesh.header.bounds), sizeof(BoundingBox));
        reader.ReadBytes(reinterpret_cast<uint8_t*>(&mesh.header.sphere), sizeof(BoundingSphere));

        // Read vertices
        if (mesh.header.hasSkeleton) {
            mesh.skinnedVertices.resize(mesh.header.vertexCount);
            if (mesh.header.vertexCount > 0) {
                reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.skinnedVertices.data()), mesh.header.vertexCount * sizeof(OmnixSkinnedVertex));
            }
            mesh.vertices.clear();
        } else {
            mesh.vertices.resize(mesh.header.vertexCount);
            if (mesh.header.vertexCount > 0) {
                reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.vertices.data()), mesh.header.vertexCount * sizeof(OmnixVertex));
            }
            mesh.skinnedVertices.clear();
        }

        // Read indices
        mesh.indices.resize(mesh.header.indexCount);
        if (mesh.header.indexCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.indices.data()), mesh.header.indexCount * sizeof(uint32_t));
        }

        // Read submeshes
        mesh.submeshes.resize(mesh.header.submeshCount);
        if (mesh.header.submeshCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.submeshes.data()), mesh.header.submeshCount * sizeof(OmnixSubmesh));
        }

        // Read material slots
        mesh.materialSlots.resize(mesh.header.materialSlotCount);
        for (uint32_t i = 0; i < mesh.header.materialSlotCount; ++i) {
            mesh.materialSlots[i] = AssetHandle{reader.ReadU64()};
        }

        // Optional skeleton
        mesh.skeletonAssetPath = reader.ReadString();

    } catch (const std::exception&) {
        return false;
    }

    return true;
}

inline bool DeserializeMeshFromMemory(OmnixMesh& mesh, const uint8_t* data, size_t size) {
    eng::runtime::BinaryReader reader;
    if (!reader.LoadFromMemory(data, size)) {
        return false;
    }

    if (!reader.ValidateHeaderAndChecksum(MAGIC_MESH, OMNIX_MESH_VERSION_MAJOR, OMNIX_MESH_VERSION_MINOR)) {
        return false;
    }

    try {
        mesh.header.vertexCount = reader.ReadU32();
        mesh.header.indexCount = reader.ReadU32();
        mesh.header.vertexStride = reader.ReadU32();
        mesh.header.submeshCount = reader.ReadU32();
        mesh.header.hasSkeleton = reader.ReadU32();
        mesh.header.materialSlotCount = reader.ReadU32();

        reader.ReadBytes(reinterpret_cast<uint8_t*>(&mesh.header.bounds), sizeof(BoundingBox));
        reader.ReadBytes(reinterpret_cast<uint8_t*>(&mesh.header.sphere), sizeof(BoundingSphere));

        // Read vertices
        if (mesh.header.hasSkeleton) {
            mesh.skinnedVertices.resize(mesh.header.vertexCount);
            if (mesh.header.vertexCount > 0) {
                reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.skinnedVertices.data()), mesh.header.vertexCount * sizeof(OmnixSkinnedVertex));
            }
            mesh.vertices.clear();
        } else {
            mesh.vertices.resize(mesh.header.vertexCount);
            if (mesh.header.vertexCount > 0) {
                reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.vertices.data()), mesh.header.vertexCount * sizeof(OmnixVertex));
            }
            mesh.skinnedVertices.clear();
        }

        // Read indices
        mesh.indices.resize(mesh.header.indexCount);
        if (mesh.header.indexCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.indices.data()), mesh.header.indexCount * sizeof(uint32_t));
        }

        // Read submeshes
        mesh.submeshes.resize(mesh.header.submeshCount);
        if (mesh.header.submeshCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(mesh.submeshes.data()), mesh.header.submeshCount * sizeof(OmnixSubmesh));
        }

        // Read material slots
        mesh.materialSlots.resize(mesh.header.materialSlotCount);
        for (uint32_t i = 0; i < mesh.header.materialSlotCount; ++i) {
            mesh.materialSlots[i] = AssetHandle{reader.ReadU64()};
        }

        // Optional skeleton
        mesh.skeletonAssetPath = reader.ReadString();

    } catch (const std::exception&) {
        return false;
    }

    return true;
}
