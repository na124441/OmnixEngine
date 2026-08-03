#include "Core/pch.h"
#include "RenderingEngine/Renderer/scene/ModelLoader.h"
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
    std::vector<glm::vec2> temp_texcoords;
    std::vector<glm::vec3> temp_normals;

    auto resolveIndex = [](int objIndex, size_t count) -> int {
        if (objIndex > 0) return objIndex - 1;
        if (objIndex < 0) return static_cast<int>(count) + objIndex;
        return -1;
    };

    struct FaceVertex {
        int position = -1;
        int uv = -1;
        int normal = -1;
    };

    auto parseFaceVertex = [&](const std::string& token) -> FaceVertex {
        FaceVertex v{};
        size_t firstSlash = token.find('/');
        size_t secondSlash = firstSlash == std::string::npos ? std::string::npos : token.find('/', firstSlash + 1);

        std::string posStr = firstSlash == std::string::npos ? token : token.substr(0, firstSlash);
        std::string uvStr = firstSlash == std::string::npos ? std::string() :
            (secondSlash == std::string::npos ? token.substr(firstSlash + 1) : token.substr(firstSlash + 1, secondSlash - firstSlash - 1));
        std::string normStr = secondSlash == std::string::npos ? std::string() : token.substr(secondSlash + 1);

        if (!posStr.empty()) v.position = resolveIndex(std::stoi(posStr), temp_positions.size());
        if (!uvStr.empty()) v.uv = resolveIndex(std::stoi(uvStr), temp_texcoords.size());
        if (!normStr.empty()) v.normal = resolveIndex(std::stoi(normStr), temp_normals.size());
        return v;
    };

    auto fetchPosition = [&](int index) -> glm::vec3 {
        return temp_positions[static_cast<size_t>(index)];
    };

    auto fetchUv = [&](int index) -> glm::vec2 {
        return temp_texcoords[static_cast<size_t>(index)];
    };

    auto fetchNormal = [&](int index) -> glm::vec3 {
        return temp_normals[static_cast<size_t>(index)];
    };

    auto computePlanarUv = [](const glm::vec3& normal, const glm::vec3& p) -> glm::vec2 {
        float absX = std::abs(normal.x);
        float absY = std::abs(normal.y);
        float absZ = std::abs(normal.z);
        if (absX >= absY && absX >= absZ) return { p.z, p.y };
        if (absY >= absX && absY >= absZ) return { p.x, p.z };
        return { p.x, p.y };
    };

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            glm::vec3 pos{};
            ss >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        } else if (prefix == "vt") {
            glm::vec2 uv{};
            ss >> uv.x >> uv.y;
            temp_texcoords.push_back(uv);
        } else if (prefix == "vn") {
            glm::vec3 normal{};
            ss >> normal.x >> normal.y >> normal.z;
            if (glm::length(normal) > 0.0001f) {
                normal = glm::normalize(normal);
            }
            temp_normals.push_back(normal);
        } else if (prefix == "f") {
            std::vector<FaceVertex> face;
            std::string token;
            while (ss >> token) {
                try {
                    face.push_back(parseFaceVertex(token));
                } catch (const std::exception&) {
                    ::Logger::Log(::LogLevel::Error, "Malformed face vertex in OBJ: " + path);
                    return false;
                }
            }

            if (face.size() < 3) {
                continue;
            }

            for (size_t tri = 1; tri + 1 < face.size(); ++tri) {
                FaceVertex refs[3] = { face[0], face[tri], face[tri + 1] };
                glm::vec3 positions[3];
                glm::vec2 uvs[3];
                glm::vec3 normals[3];
                bool hasFaceNormals = true;
                bool hasFaceUvs = true;

                for (int i = 0; i < 3; ++i) {
                    if (refs[i].position < 0 || static_cast<size_t>(refs[i].position) >= temp_positions.size()) {
                        ::Logger::Log(::LogLevel::Error, "Malformed face index in OBJ: " + path + " - position index out of range");
                        return false;
                    }
                    positions[i] = fetchPosition(refs[i].position);

                    if (refs[i].uv >= 0 && static_cast<size_t>(refs[i].uv) < temp_texcoords.size()) {
                        uvs[i] = fetchUv(refs[i].uv);
                    } else {
                        hasFaceUvs = false;
                    }

                    if (refs[i].normal >= 0 && static_cast<size_t>(refs[i].normal) < temp_normals.size()) {
                        normals[i] = fetchNormal(refs[i].normal);
                    } else {
                        hasFaceNormals = false;
                    }
                }

                glm::vec3 e1 = positions[1] - positions[0];
                glm::vec3 e2 = positions[2] - positions[0];
                glm::vec3 faceNormal = {0.0f, 0.0f, 1.0f};
                if (glm::length(e1) > 0.0001f && glm::length(e2) > 0.0001f) {
                    faceNormal = glm::normalize(glm::cross(e1, e2));
                }

                if (!hasFaceNormals) {
                    normals[0] = normals[1] = normals[2] = faceNormal;
                }

                if (!hasFaceUvs) {
                    uvs[0] = computePlanarUv(faceNormal, positions[0]);
                    uvs[1] = computePlanarUv(faceNormal, positions[1]);
                    uvs[2] = computePlanarUv(faceNormal, positions[2]);
                }

                glm::vec3 tangent{1.0f, 0.0f, 0.0f};
                glm::vec2 duv1 = uvs[1] - uvs[0];
                glm::vec2 duv2 = uvs[2] - uvs[0];
                float denom = (duv1.x * duv2.y) - (duv2.x * duv1.y);
                if (std::abs(denom) > 0.00001f) {
                    float f = 1.0f / denom;
                    tangent = {
                        f * (duv2.y * e1.x - duv1.y * e2.x),
                        f * (duv2.y * e1.y - duv1.y * e2.y),
                        f * (duv2.y * e1.z - duv1.y * e2.z)
                    };
                    if (glm::length(tangent) > 0.0001f) {
                        tangent = glm::normalize(tangent);
                    }
                }

                for (int i = 0; i < 3; ++i) {
                    indices.push_back(static_cast<uint32_t>(vertices.size()));
                    vertices.push_back({ positions[i], normals[i], uvs[i], glm::vec4(tangent, 1.0f) });
                }
            }
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