#include "Core/pch.h"
#include "ModelLoader.h"
#include "Core/Engine/Log.h"

// Fix collisions between Core Logger and Rendering Logger
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR
#undef LOG_FATAL
#include "Core/Logger.h" 

#include "Core/Engine/EngineResources.h"
#include "Core/types/Vertex.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>

namespace eng::renderer {

MeshBounds ComputeMeshBounds(const std::vector<PbrVertex>& vertices)
{
    MeshBounds bounds{};
    if (vertices.empty())
    {
        bounds.localCenter = glm::vec3(0.0f);
        bounds.localRadius = 1.0f;
        return bounds;
    }
    glm::vec3 minP = vertices[0].position;
    glm::vec3 maxP = vertices[0].position;
    for (const auto& v : vertices)
    {
        minP = glm::min(minP, v.position);
        maxP = glm::max(maxP, v.position);
    }
    bounds.localMin = minP;
    bounds.localMax = maxP;
    bounds.localCenter = (minP + maxP) * 0.5f;
    float radius = 0.0f;
    for (const auto& v : vertices)
    {
        radius = glm::max(radius, glm::length(v.position - bounds.localCenter));
    }
    bounds.localRadius = radius;
    return bounds;
}

bool ModelLoader::LoadOBJ(const std::string& path, Mesh& outMesh, EngineResources& resources) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ::Logger::Log(::LogLevel::Error, "Failed to open model file: " + path);
        return false;
    }

    std::vector<PbrVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> temp_positions;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        } else if (prefix == "f") {
            std::string v1, v2, v3;
            if (!(ss >> v1 >> v2 >> v3)) continue;

            auto parse_index = [&](const std::string& v_str) -> uint32_t {
                size_t slash = v_str.find('/');
                std::string idx_str = (slash == std::string::npos) ? v_str : v_str.substr(0, slash);
                return static_cast<uint32_t>(std::stoul(idx_str)) - 1;
            };

            uint32_t i1 = parse_index(v1);
            uint32_t i2 = parse_index(v2);
            uint32_t i3 = parse_index(v3);

            if (i1 >= temp_positions.size() || i2 >= temp_positions.size() || i3 >= temp_positions.size()) {
                ::Logger::Log(::LogLevel::Error, "Malformed face index in OBJ: " + path + " - index out of range of positions (positions size: " + std::to_string(temp_positions.size()) + ")");
                return false;
            }

            glm::vec3 p1 = temp_positions[i1];
            glm::vec3 p2 = temp_positions[i2];
            glm::vec3 p3 = temp_positions[i3];

            glm::vec3 e1 = p2 - p1;
            glm::vec3 e2 = p3 - p1;
            glm::vec3 faceNormal = { 0.0f, 0.0f, 1.0f };
            if (glm::length(e1) > 0.0001f && glm::length(e2) > 0.0001f) {
                faceNormal = glm::normalize(glm::cross(e1, e2));
            }

            // Determine dominant axis of normal for planar projection
            float absX = std::abs(faceNormal.x);
            float absY = std::abs(faceNormal.y);
            float absZ = std::abs(faceNormal.z);

            glm::vec2 uv1, uv2, uv3;
            if (absX >= absY && absX >= absZ) {
                uv1 = { p1.z, p1.y };
                uv2 = { p2.z, p2.y };
                uv3 = { p3.z, p3.y };
            } else if (absY >= absX && absY >= absZ) {
                uv1 = { p1.x, p1.z };
                uv2 = { p2.x, p2.z };
                uv3 = { p3.x, p3.z };
            } else {
                uv1 = { p1.x, p1.y };
                uv2 = { p2.x, p2.y };
                uv3 = { p3.x, p3.y };
            }

            // Scale UV coordinates slightly to fit nicely on standard geometries
            float uvScale = 2.0f;
            uv1 *= uvScale;
            uv2 *= uvScale;
            uv3 *= uvScale;

            // Compute tangent vectors for proper normal map perturbation
            glm::vec3 tangent{1.0f, 0.0f, 0.0f};
            glm::vec2 duv1 = uv2 - uv1;
            glm::vec2 duv2 = uv3 - uv1;
            float denom = (duv1.x * duv2.y - duv2.x * duv1.y);
            if (std::abs(denom) > 0.00001f) {
                float f = 1.0f / denom;
                tangent.x = f * (duv2.y * e1.x - duv1.y * e2.x);
                tangent.y = f * (duv2.y * e1.y - duv1.y * e2.y);
                tangent.z = f * (duv2.y * e1.z - duv1.y * e2.z);
                if (glm::length(tangent) > 0.0001f) {
                    tangent = glm::normalize(tangent);
                } else {
                    tangent = {1.0f, 0.0f, 0.0f};
                }
            }
            glm::vec4 tVal = glm::vec4(tangent, 1.0f);

            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({p1, faceNormal, uv1, tVal});

            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({p2, faceNormal, uv2, tVal});

            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({p3, faceNormal, uv3, tVal});
        }
    }

    if (vertices.empty()) {
        ::Logger::Log(::LogLevel::Error, "No geometry found in OBJ file: " + path);
        return false;
    }

    outMesh.bounds = ComputeMeshBounds(vertices);
    outMesh.minBounds = outMesh.bounds.localMin;
    outMesh.maxBounds = outMesh.bounds.localMax;

    ::Logger::Log(::LogLevel::Info, "Loaded OBJ: " + path + " (" + std::to_string(vertices.size()) + " vertices)");
    ::Logger::Log(::LogLevel::Info, "Model Bounds: Min(" + std::to_string(outMesh.minBounds.x) + "," + std::to_string(outMesh.minBounds.y) + "," + std::to_string(outMesh.minBounds.z) + ") Max(" + std::to_string(outMesh.maxBounds.x) + "," + std::to_string(outMesh.maxBounds.y) + "," + std::to_string(outMesh.maxBounds.z) + ")");

    outMesh.hasNormals = true;
    outMesh.hasUVs = true;
    outMesh.hasTangents = true;

    return outMesh.init(vertices.data(), vertices.size(), indices.data(), indices.size(), resources);
}

} // namespace eng::renderer
