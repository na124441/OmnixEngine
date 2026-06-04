#pragma once
#include "Runtime/Public/OmnixMeshFormat.h"
#include "Runtime/Public/MeshMetadata.h"
#include <string>
#include <vector>

namespace eng::runtime {

    class MeshImporter
    {
    public:
        MeshImporter() = default;
        ~MeshImporter() = default;

        /**
         * @brief Core import function. Loads OBJ/GLTF, generates normals/tangents, calculates bounds,
         * validates integrity, saves `.omnixmesh` and sidecar metadata `.meta` to disk.
         * Returns true if successful, false otherwise.
         */
        bool ImportMesh(const std::string& sourcePath, const std::string& cachePath, MeshMetadata& outMetadata, bool forceReimport = false);

        /**
         * @brief Generates vertex normals by accumulating face normals of connected triangles.
         */
        void GenerateNormals(const std::vector<Vec3>& positions, const std::vector<uint32_t>& indices, std::vector<Vec3>& outNormals);

        /**
         * @brief Generates tangent vectors from UVs and normals.
         */
        void GenerateTangents(const std::vector<Vec3>& positions, const std::vector<Vec3>& normals, const std::vector<Vec2>& uvs, const std::vector<uint32_t>& indices, std::vector<Vec4>& outTangents);
    };

} // namespace eng::runtime
