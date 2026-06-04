#include "Runtime/Public/OBJImporter.h"
#include "Core/Logger.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace eng::runtime {

    struct OBJVertexKey
    {
        int32_t posIdx = 0;
        int32_t uvIdx = 0;
        int32_t normIdx = 0;

        bool operator==(const OBJVertexKey& o) const noexcept {
            return posIdx == o.posIdx && uvIdx == o.uvIdx && normIdx == o.normIdx;
        }
    };

    struct OBJVertexKeyHash
    {
        std::size_t operator()(const OBJVertexKey& k) const noexcept {
            std::size_t h1 = std::hash<int32_t>{}(k.posIdx);
            std::size_t h2 = std::hash<int32_t>{}(k.uvIdx);
            std::size_t h3 = std::hash<int32_t>{}(k.normIdx);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    static void ParseOBJFaceToken(const std::string& token, int32_t& posIdx, int32_t& uvIdx, int32_t& normIdx) noexcept {
        posIdx = 0;
        uvIdx = 0;
        normIdx = 0;

        std::stringstream ss(token);
        std::string part;

        // Parse position index
        if (std::getline(ss, part, '/')) {
            if (!part.empty()) {
                try { posIdx = std::stoi(part); } catch (...) {}
            }
        }
        // Parse UV index
        if (std::getline(ss, part, '/')) {
            if (!part.empty()) {
                try { uvIdx = std::stoi(part); } catch (...) {}
            }
        }
        // Parse normal index
        if (std::getline(ss, part, '/')) {
            if (!part.empty()) {
                try { normIdx = std::stoi(part); } catch (...) {}
            }
        }
    }

    bool ParseOBJ(const std::string& path, RawOBJData& outData) {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("[OBJImporter] Failed to open OBJ file: %s", path.c_str());
            return false;
        }

        std::vector<Vec3> temp_positions;
        std::vector<Vec3> temp_normals;
        std::vector<Vec2> temp_uvs;

        std::unordered_map<OBJVertexKey, uint32_t, OBJVertexKeyHash> uniqueVertices;

        outData.positions.clear();
        outData.normals.clear();
        outData.uvs.clear();
        outData.indices.clear();
        outData.hasNormals = false;
        outData.hasUVs = false;

        std::string line;
        while (std::getline(file, line)) {
            // Trim leading whitespace
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));

            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::stringstream ss(line);
            std::string prefix;
            ss >> prefix;

            if (prefix == "v") {
                Vec3 p;
                ss >> p.x >> p.y >> p.z;
                temp_positions.push_back(p);
            } else if (prefix == "vn") {
                Vec3 n;
                ss >> n.x >> n.y >> n.z;
                temp_normals.push_back(n);
            } else if (prefix == "vt") {
                Vec2 uv;
                ss >> uv.x >> uv.y;
                temp_uvs.push_back(uv);
            } else if (prefix == "f") {
                std::vector<OBJVertexKey> faceVertices;
                std::string token;
                while (ss >> token) {
                    int32_t posIdx, uvIdx, normIdx;
                    ParseOBJFaceToken(token, posIdx, uvIdx, normIdx);

                    // Resolve negative relative indices or offset to 0-based
                    if (posIdx == 0) {
                        LOG_ERROR("[OBJImporter] Face token is missing position index.");
                        return false;
                    }
                    if (posIdx > 0) posIdx -= 1;
                    else if (posIdx < 0) posIdx = static_cast<int32_t>(temp_positions.size()) + posIdx;

                    if (posIdx < 0 || static_cast<size_t>(posIdx) >= temp_positions.size()) {
                        LOG_ERROR("[OBJImporter] Position index out of bounds.");
                        return false;
                    }

                    if (uvIdx != 0) {
                        if (uvIdx > 0) uvIdx -= 1;
                        else if (uvIdx < 0) uvIdx = static_cast<int32_t>(temp_uvs.size()) + uvIdx;

                        if (uvIdx < 0 || static_cast<size_t>(uvIdx) >= temp_uvs.size()) {
                            LOG_ERROR("[OBJImporter] UV index out of bounds.");
                            return false;
                        }
                    } else {
                        uvIdx = -1;
                    }

                    if (normIdx != 0) {
                        if (normIdx > 0) normIdx -= 1;
                        else if (normIdx < 0) normIdx = static_cast<int32_t>(temp_normals.size()) + normIdx;

                        if (normIdx < 0 || static_cast<size_t>(normIdx) >= temp_normals.size()) {
                            LOG_ERROR("[OBJImporter] Normal index out of bounds.");
                            return false;
                        }
                    } else {
                        normIdx = -1;
                    }

                    faceVertices.push_back({ posIdx, uvIdx, normIdx });
                }

                if (faceVertices.size() < 3) {
                    continue; // Skip invalid faces
                }

                // Perform fan triangulation for convex polygons
                for (size_t i = 1; i < faceVertices.size() - 1; ++i) {
                    OBJVertexKey tri[3] = {
                        faceVertices[0],
                        faceVertices[i],
                        faceVertices[i + 1]
                    };

                    for (int j = 0; j < 3; ++j) {
                        const auto& key = tri[j];
                        auto it = uniqueVertices.find(key);
                        if (it != uniqueVertices.end()) {
                            outData.indices.push_back(it->second);
                        } else {
                            uint32_t newIndex = static_cast<uint32_t>(outData.positions.size());
                            uniqueVertices[key] = newIndex;
                            outData.indices.push_back(newIndex);

                            // Store position
                            if (key.posIdx >= 0 && static_cast<size_t>(key.posIdx) < temp_positions.size()) {
                                outData.positions.push_back(temp_positions[key.posIdx]);
                            } else {
                                outData.positions.push_back({ 0, 0, 0 });
                            }

                            // Store normal
                            if (key.normIdx >= 0 && static_cast<size_t>(key.normIdx) < temp_normals.size()) {
                                outData.normals.push_back(temp_normals[key.normIdx]);
                                outData.hasNormals = true;
                            } else {
                                outData.normals.push_back({ 0, 0, 0 });
                            }

                            // Store UV
                            if (key.uvIdx >= 0 && static_cast<size_t>(key.uvIdx) < temp_uvs.size()) {
                                outData.uvs.push_back(temp_uvs[key.uvIdx]);
                                outData.hasUVs = true;
                            } else {
                                outData.uvs.push_back({ 0, 0 });
                            }
                        }
                    }
                }
            }
        }

        file.close();

        if (outData.positions.empty()) {
            LOG_ERROR("[OBJImporter] No valid geometry parsed from file: %s", path.c_str());
            return false;
        }

        return true;
    }

} // namespace eng::runtime
