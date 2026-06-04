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

bool ModelLoader::LoadOBJ(const std::string& path, Mesh& outMesh, EngineResources& resources) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ::Logger::Log(::LogLevel::Error, "Failed to open model file: " + path);
        return false;
    }

    std::vector<Vertex> vertices;
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

            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({temp_positions[i1], {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});

            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({temp_positions[i2], {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});

            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({temp_positions[i3], {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
        }
    }

    if (vertices.empty()) {
        ::Logger::Log(::LogLevel::Error, "No geometry found in OBJ file: " + path);
        return false;
    }

    glm::vec3 min(1e10f), max(-1e10f);
    for (const auto& v : vertices) {
        min = glm::min(min, v.pos);
        max = glm::max(max, v.pos);
    }

    ::Logger::Log(::LogLevel::Info, "Loaded OBJ: " + path + " (" + std::to_string(vertices.size()) + " vertices)");
    ::Logger::Log(::LogLevel::Info, "Model Bounds: Min(" + std::to_string(min.x) + "," + std::to_string(min.y) + "," + std::to_string(min.z) + ") Max(" + std::to_string(max.x) + "," + std::to_string(max.y) + "," + std::to_string(max.z) + ")");

    outMesh.minBounds = min;
    outMesh.maxBounds = max;

    return outMesh.init(vertices.data(), vertices.size(), indices.data(), indices.size(), resources);
}

} // namespace eng::renderer
