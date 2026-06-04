#include "Runtime/Public/MeshImporter.h"
#include "Runtime/Public/OBJImporter.h"
#include "Runtime/Public/GLTFImporter.h"
#include "Runtime/Public/MeshValidation.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Core/Logger.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace eng::runtime {

    // Helper math operations for Vec3 and Vec2
    static inline Vec3 Add(const Vec3& a, const Vec3& b) noexcept {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }
    static inline Vec3 Sub(const Vec3& a, const Vec3& b) noexcept {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }
    static inline Vec3 Mul(const Vec3& a, float s) noexcept {
        return { a.x * s, a.y * s, a.z * s };
    }
    static inline float Dot(const Vec3& a, const Vec3& b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static inline Vec3 Cross(const Vec3& a, const Vec3& b) noexcept {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
    static inline float Length(const Vec3& v) noexcept {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
    static inline Vec3 Normalize(const Vec3& v) noexcept {
        float len = Length(v);
        if (len > 0.0f) {
            return { v.x / len, v.y / len, v.z / len };
        }
        return { 0, 0, 0 };
    }

    static uint64_t GetFileLastWriteTime(const std::string& filepath) noexcept {
        try {
            if (!std::filesystem::exists(filepath)) {
                return 0;
            }
            auto ftime = std::filesystem::last_write_time(filepath);
            return std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
        } catch (...) {
            return 0;
        }
    }

    bool MeshImporter::ImportMesh(const std::string& sourcePath, const std::string& cachePath, MeshMetadata& outMetadata, bool forceReimport) {
        // Caching validation
        if (!forceReimport && std::filesystem::exists(cachePath) && std::filesystem::exists(cachePath + ".meta")) {
            try {
                auto sourceTime = std::filesystem::last_write_time(sourcePath);
                auto cacheTime = std::filesystem::last_write_time(cachePath);
                if (sourceTime <= cacheTime) {
                    if (LoadMeshMetadata(outMetadata, cachePath + ".meta")) {
                        LOG_INFO("[MeshImporter] Using cached mesh: %s", cachePath.c_str());
                        return true;
                    }
                }
            } catch (...) {}
        }

        LOG_INFO("[MeshImporter] Importing mesh: %s -> %s", sourcePath.c_str(), cachePath.c_str());

        std::string ext = std::filesystem::path(sourcePath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        std::vector<Vec3> positions;
        std::vector<Vec3> normals;
        std::vector<Vec4> tangents;
        std::vector<Vec2> uvs;
        std::vector<uint32_t> indices;
        std::vector<OmnixSubmesh> submeshes;

        bool hasNormals = false;
        bool hasTangents = false;
        bool hasUVs = false;

        if (ext == ".obj") {
            RawOBJData objData;
            if (!ParseOBJ(sourcePath, objData)) {
                LOG_ERROR("[MeshImporter] Failed to parse OBJ file: %s", sourcePath.c_str());
                return false;
            }
            positions = std::move(objData.positions);
            normals = std::move(objData.normals);
            uvs = std::move(objData.uvs);
            indices = std::move(objData.indices);

            // OBJ is one submesh by default for v0.2
            OmnixSubmesh sub;
            sub.indexStart = 0;
            sub.indexCount = static_cast<uint32_t>(indices.size());
            sub.materialIndex = 0;
            submeshes.push_back(sub);

            hasNormals = objData.hasNormals;
            hasTangents = false; // OBJ doesn't store tangents
            hasUVs = objData.hasUVs;

        } else if (ext == ".gltf" || ext == ".glb") {
            RawGLTFData gltfData;
            if (!ParseGLTF(sourcePath, gltfData)) {
                LOG_ERROR("[MeshImporter] Failed to parse GLTF file: %s", sourcePath.c_str());
                return false;
            }
            positions = std::move(gltfData.positions);
            normals = std::move(gltfData.normals);
            tangents = std::move(gltfData.tangents);
            uvs = std::move(gltfData.uvs);
            indices = std::move(gltfData.indices);

            for (const auto& sub : gltfData.submeshes) {
                submeshes.push_back({ sub.indexStart, sub.indexCount, sub.materialIndex });
            }

            hasNormals = gltfData.hasNormals;
            hasTangents = gltfData.hasTangents;
            hasUVs = gltfData.hasUVs;
        } else {
            LOG_ERROR("[MeshImporter] Unsupported mesh format: %s", ext.c_str());
            return false;
        }

        // Generate normals if missing
        if (!hasNormals) {
            LOG_INFO("[MeshImporter] Missing normals. Generating normals...");
            GenerateNormals(positions, indices, normals);
            hasNormals = true;
        }

        // Generate tangents if missing (requires UVs)
        if (!hasTangents) {
            if (hasUVs) {
                LOG_INFO("[MeshImporter] Missing tangents. Generating tangents...");
                GenerateTangents(positions, normals, uvs, indices, tangents);
                hasTangents = true;
            } else {
                // If no UVs, fill tangents with default basis
                tangents.resize(positions.size(), { 1.0f, 0.0f, 0.0f, 1.0f });
            }
        }

        // Calculate Bounding Box (AABB)
        BoundingBox bounds;
        bounds.min = { 1e10f, 1e10f, 1e10f };
        bounds.max = { -1e10f, -1e10f, -1e10f };
        for (const auto& p : positions) {
            bounds.min.x = std::min(bounds.min.x, p.x);
            bounds.min.y = std::min(bounds.min.y, p.y);
            bounds.min.z = std::min(bounds.min.z, p.z);

            bounds.max.x = std::max(bounds.max.x, p.x);
            bounds.max.y = std::max(bounds.max.y, p.y);
            bounds.max.z = std::max(bounds.max.z, p.z);
        }

        // Calculate Bounding Sphere
        BoundingSphere sphere;
        sphere.center = Add(bounds.min, bounds.max);
        sphere.center = Mul(sphere.center, 0.5f);

        float maxDistSq = 0.0f;
        for (const auto& p : positions) {
            Vec3 diff = Sub(p, sphere.center);
            float distSq = Dot(diff, diff);
            maxDistSq = std::max(maxDistSq, distSq);
        }
        sphere.radius = std::sqrt(maxDistSq);

        // Populate OmnixMesh Struct
        OmnixMesh mesh;
        mesh.header.vertexCount = static_cast<uint32_t>(positions.size());
        mesh.header.indexCount = static_cast<uint32_t>(indices.size());
        mesh.header.vertexStride = sizeof(OmnixVertex);
        mesh.header.submeshCount = static_cast<uint32_t>(submeshes.size());
        mesh.header.hasSkeleton = 0;
        mesh.header.materialSlotCount = static_cast<uint32_t>(submeshes.size());
        mesh.header.bounds = bounds;
        mesh.header.sphere = sphere;

        // Populate vertex streams
        mesh.vertices.resize(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) {
            mesh.vertices[i].position = positions[i];
            mesh.vertices[i].normal = normals[i];
            mesh.vertices[i].tangent = tangents[i];
            mesh.vertices[i].uv0 = uvs[i];
            mesh.vertices[i].uv1 = { 0.0f, 0.0f }; // pad uv1
        }
        mesh.indices = std::move(indices);
        mesh.submeshes = std::move(submeshes);

        // Map material slots
        mesh.materialSlots.resize(mesh.header.materialSlotCount);
        for (uint32_t i = 0; i < mesh.header.materialSlotCount; ++i) {
            mesh.materialSlots[i] = AssetHandle{ i }; // Default slots mapped by index
        }

        // Validate Mesh Integrity
        if (!ValidateMeshIntegrity(mesh)) {
            LOG_ERROR("[MeshImporter] Mesh integrity validation failed for: %s", sourcePath.c_str());
            return false;
        }

        // Export binary cached file
        std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());
        if (!SerializeMesh(mesh, cachePath)) {
            LOG_ERROR("[MeshImporter] Failed to serialize cached .omnixmesh to: %s", cachePath.c_str());
            return false;
        }

        // Populate Metadata
        outMetadata.handle = GenerateAssetUUID(sourcePath, AssetType::Mesh);
        outMetadata.vertexCount = mesh.header.vertexCount;
        outMetadata.indexCount = mesh.header.indexCount;
        outMetadata.submeshCount = mesh.header.submeshCount;
        outMetadata.bounds = bounds;
        outMetadata.sphere = sphere;
        outMetadata.hasNormals = hasNormals;
        outMetadata.hasTangents = hasTangents;
        outMetadata.hasUV0 = hasUVs;
        outMetadata.hasSkeleton = false;
        outMetadata.sourceTimestamp = GetFileLastWriteTime(sourcePath);
        outMetadata.importTimestamp = GetFileLastWriteTime(cachePath);

        // Save Metadata sidecar
        if (!SaveMeshMetadata(outMetadata, cachePath + ".meta")) {
            LOG_ERROR("[MeshImporter] Failed to save sidecar metadata file: %s.meta", cachePath.c_str());
            return false;
        }

        return true;
    }

    void MeshImporter::GenerateNormals(const std::vector<Vec3>& positions, const std::vector<uint32_t>& indices, std::vector<Vec3>& outNormals) {
        outNormals.clear();
        outNormals.resize(positions.size(), { 0.0f, 0.0f, 0.0f });

        // Accumulate face normals
        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t idx0 = indices[i];
            uint32_t idx1 = indices[i + 1];
            uint32_t idx2 = indices[i + 2];

            Vec3 p0 = positions[idx0];
            Vec3 p1 = positions[idx1];
            Vec3 p2 = positions[idx2];

            Vec3 edge1 = Sub(p1, p0);
            Vec3 edge2 = Sub(p2, p0);

            Vec3 faceNormal = Cross(edge1, edge2);

            outNormals[idx0] = Add(outNormals[idx0], faceNormal);
            outNormals[idx1] = Add(outNormals[idx1], faceNormal);
            outNormals[idx2] = Add(outNormals[idx2], faceNormal);
        }

        // Normalize
        for (size_t i = 0; i < outNormals.size(); ++i) {
            Vec3 n = Normalize(outNormals[i]);
            if (Length(n) == 0.0f) {
                n = { 0.0f, 1.0f, 0.0f }; // Fallback normal
            }
            outNormals[i] = n;
        }
    }

    void MeshImporter::GenerateTangents(const std::vector<Vec3>& positions, const std::vector<Vec3>& normals, const std::vector<Vec2>& uvs, const std::vector<uint32_t>& indices, std::vector<Vec4>& outTangents) {
        outTangents.clear();
        outTangents.resize(positions.size(), { 0.0f, 0.0f, 0.0f, 1.0f });

        std::vector<Vec3> tan1(positions.size(), { 0.0f, 0.0f, 0.0f });
        std::vector<Vec3> tan2(positions.size(), { 0.0f, 0.0f, 0.0f });

        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t i1 = indices[i];
            uint32_t i2 = indices[i + 1];
            uint32_t i3 = indices[i + 2];

            const Vec3& v1 = positions[i1];
            const Vec3& v2 = positions[i2];
            const Vec3& v3 = positions[i3];

            const Vec2& w1 = uvs[i1];
            const Vec2& w2 = uvs[i2];
            const Vec2& w3 = uvs[i3];

            float x1 = v2.x - v1.x;
            float x2 = v3.x - v1.x;
            float y1 = v2.y - v1.y;
            float y2 = v3.y - v1.y;
            float z1 = v2.z - v1.z;
            float z2 = v3.z - v1.z;

            float s1 = w2.x - w1.x;
            float s2 = w3.x - w1.x;
            float t1 = w2.y - w1.y;
            float t2 = w3.y - w1.y;

            float r = (s1 * t2 - s2 * t1);
            float div = (r == 0.0f) ? 1.0f : 1.0f / r;

            Vec3 sdir = {
                (t2 * x1 - t1 * x2) * div,
                (t2 * y1 - t1 * y2) * div,
                (t2 * z1 - t1 * z2) * div
            };

            Vec3 tdir = {
                (s1 * x2 - s2 * x1) * div,
                (s1 * y2 - s2 * y1) * div,
                (s1 * z2 - s2 * z1) * div
            };

            tan1[i1] = Add(tan1[i1], sdir);
            tan1[i2] = Add(tan1[i2], sdir);
            tan1[i3] = Add(tan1[i3], sdir);

            tan2[i1] = Add(tan2[i1], tdir);
            tan2[i2] = Add(tan2[i2], tdir);
            tan2[i3] = Add(tan2[i3], tdir);
        }

        // Gram-Schmidt orthogonalization
        for (size_t a = 0; a < positions.size(); ++a) {
            const Vec3& n = normals[a];
            const Vec3& t = tan1[a];

            // Gram-Schmidt orthogonalize: t_ortho = normalize(t - n * dot(n, t))
            Vec3 t_ortho = Normalize(Sub(t, Mul(n, Dot(n, t))));

            // Handedness calculation: w = dot(cross(n, t_ortho), tan2[a]) < 0 ? -1 : 1
            float w = (Dot(Cross(n, t_ortho), tan2[a]) < 0.0f) ? -1.0f : 1.0f;

            outTangents[a] = { t_ortho.x, t_ortho.y, t_ortho.z, w };
        }
    }

} // namespace eng::runtime
