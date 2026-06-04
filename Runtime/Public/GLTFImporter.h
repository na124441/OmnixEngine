#pragma once
#include "Runtime/Public/OmnixMeshFormat.h"
#include <string>
#include <vector>

namespace eng::runtime {

    struct RawGLTFSubmesh
    {
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
        uint32_t materialIndex = 0;
    };

    struct RawGLTFData
    {
        std::vector<Vec3> positions;
        std::vector<Vec3> normals;
        std::vector<Vec4> tangents;
        std::vector<Vec2> uvs;
        std::vector<uint32_t> indices;
        std::vector<RawGLTFSubmesh> submeshes;

        bool hasNormals = false;
        bool hasTangents = false;
        bool hasUVs = false;
    };

    /**
     * @brief Parses GLTF meshes (ASCII/Binary) and extracts geometry buffers.
     */
    bool ParseGLTF(const std::string& path, RawGLTFData& outData);

} // namespace eng::runtime
