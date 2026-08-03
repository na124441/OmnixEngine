#pragma once
#include "Runtime/OmnixMeshFormat.h" // reuse Vec3, Vec2, etc.
#include <string>
#include <vector>

namespace eng::runtime {

    struct RawOBJData
    {
        std::vector<Vec3> positions;
        std::vector<Vec3> normals;
        std::vector<Vec2> uvs;
        std::vector<uint32_t> indices;

        bool hasNormals = false;
        bool hasUVs = false;
    };

    /**
     * @brief Parses OBJ file, normalizes indexing, and performs fan triangulation on polygons.
     */
    bool ParseOBJ(const std::string& path, RawOBJData& outData);

} // namespace eng::runtime
