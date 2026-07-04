#include "MeshCanonicalizer.h"
#include "MeshAdjacency.h"
#include "ClusterBuilder.h"
#include "HierarchyBuilder.h"
#include "RVGPagePacker.h"
#include "RVGWriter.h"
#include "Core/Engine/Log.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace eng::cooker {

struct TempVertex {
    uint32_t posIdx = 0xFFFFFFFF;
    uint32_t uvIdx = 0xFFFFFFFF;
    uint32_t normalIdx = 0xFFFFFFFF;

    bool operator==(const TempVertex& other) const {
        return posIdx == other.posIdx && uvIdx == other.uvIdx && normalIdx == other.normalIdx;
    }
};

struct TempVertexHash {
    std::size_t operator()(const TempVertex& tv) const noexcept {
        return (std::hash<uint32_t>{}(tv.posIdx) ^ 
               (std::hash<uint32_t>{}(tv.uvIdx) << 1) ^ 
               (std::hash<uint32_t>{}(tv.normalIdx) << 2));
    }
};

bool MeshCanonicalizer::Canonicalize(const std::string& sourcePath, const std::string& outputPath, uint32_t pageSize, uint32_t vertexLimit, uint32_t triangleLimit) {
    LOG_INFO(("MeshCanonicalizer: Starting processing for: " + sourcePath).c_str());

    m_Vertices.clear();
    m_Indices.clear();
    m_RawPositions.clear();
    m_RawNormals.clear();
    m_RawUVs.clear();

    if (!loadOBJ(sourcePath)) {
        return false;
    }

    removeDuplicatesAndDegenerates();
    generateNormalsAndTangents();
    calculateBounds();

    std::cout << "MeshCanonicalizer completed:\n"
              << " - Output Vertices:  " << m_Vertices.size() << "\n"
              << " - Output Triangles: " << m_Indices.size() / 3 << "\n";

    // Build and print adjacency info
    MeshAdjacency adjacency;
    if (adjacency.BuildAdjacency(m_Vertices, m_Indices)) {
        adjacency.PrintReport();
    }

    // Build and print leaf clusters
    ClusterBuilder clusterBuilder;
    if (clusterBuilder.BuildClusters(m_Vertices, m_Indices, vertexLimit, triangleLimit)) {
        const auto& clusters = clusterBuilder.GetClusters();
        std::cout << "--- Cluster Generation Report ---\n"
                  << " - Total Leaf Clusters: " << clusters.size() << "\n";
        for (size_t i = 0; i < clusters.size(); ++i) {
            const auto& c = clusters[i];
            std::cout << "   [Cluster " << i << "]: "
                      << c.vertices.size() << " verts, "
                      << c.indices.size() / 3 << " tris, "
                      << "Sphere radius: " << c.boundsSphere.w << ", "
                      << "Cone cutoff: " << c.coneCutoff << "\n";
        }
        std::cout << "---------------------------------\n";

        // Build and print hierarchy DAG
        HierarchyBuilder hierarchyBuilder;
        if (hierarchyBuilder.BuildHierarchy(clusters, vertexLimit, triangleLimit)) {
            const auto& nodes = hierarchyBuilder.GetNodes();
            std::cout << "--- Hierarchy Builder Report ---\n"
                      << " - Total DAG Nodes: " << nodes.size() << "\n"
                      << " - Root Node ID: " << nodes.size() - 1 << "\n"
                      << " - Max Geometric Error: " << nodes.back().geometricError << "\n"
                      << "---------------------------------\n";

            // Run Page Packing on the flat list of all clusters (leaves + parent nodes)
            RVGPagePacker packer;
            if (packer.PackPages(hierarchyBuilder.GetAllClusters(), pageSize)) {
                // Determine bounding box of canonicalized mesh
                glm::vec3 boundsMin = m_Vertices[0].position;
                glm::vec3 boundsMax = m_Vertices[0].position;
                for (const auto& v : m_Vertices) {
                    boundsMin = glm::min(boundsMin, v.position);
                    boundsMax = glm::max(boundsMax, v.position);
                }

                // Write the binary `.rvg` file
                RVGWriter writer;
                if (!writer.Write(
                    outputPath,
                    hierarchyBuilder.GetAllClusters(),
                    nodes,
                    packer.GetPages(),
                    packer.GetPackedClusterInfos(),
                    boundsMin,
                    boundsMax
                )) {
                    std::cerr << "Failed to write RVG cooked binary file.\n";
                    return false;
                }
            } else {
                std::cerr << "Failed to pack virtual pages.\n";
                return false;
            }
        }
    }

    return true;
}

bool MeshCanonicalizer::loadOBJ(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR(("Failed to open mesh file: " + path).c_str());
        return false;
    }

    std::string line;
    std::vector<TempVertex> rawFaceVertices;

    // Temporary storage matching raw OBJ face vertices
    std::vector<TempVertex> loadedFaceVerts;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string type;
        iss >> type;

        if (type == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            m_RawPositions.push_back(pos);
        } else if (type == "vt") {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            m_RawUVs.push_back(uv);
        } else if (type == "vn") {
            glm::vec3 norm;
            iss >> norm.x >> norm.y >> norm.z;
            m_RawNormals.push_back(norm);
        } else if (type == "f") {
            loadedFaceVerts.clear();
            std::string vertToken;
            while (iss >> vertToken) {
                std::istringstream vss(vertToken);
                std::string part;
                TempVertex tv;

                // Position index
                if (std::getline(vss, part, '/')) {
                    if (!part.empty()) tv.posIdx = std::stoul(part) - 1;
                }
                // UV index
                if (std::getline(vss, part, '/')) {
                    if (!part.empty()) tv.uvIdx = std::stoul(part) - 1;
                }
                // Normal index
                if (std::getline(vss, part, '/')) {
                    if (!part.empty()) tv.normalIdx = std::stoul(part) - 1;
                }

                loadedFaceVerts.push_back(tv);
            }

            // Triangulate polygons (fan triangulation)
            if (loadedFaceVerts.size() >= 3) {
                for (size_t i = 1; i < loadedFaceVerts.size() - 1; ++i) {
                    rawFaceVertices.push_back(loadedFaceVerts[0]);
                    rawFaceVertices.push_back(loadedFaceVerts[i]);
                    rawFaceVertices.push_back(loadedFaceVerts[i + 1]);
                }
            }
        }
    }

    // Convert raw face vertices to intermediate PbrVertex layout
    m_Vertices.reserve(rawFaceVertices.size());
    for (size_t i = 0; i < rawFaceVertices.size(); ++i) {
        const auto& tv = rawFaceVertices[i];
        eng::renderer::PbrVertex vert{};

        if (tv.posIdx < m_RawPositions.size()) {
            vert.position = m_RawPositions[tv.posIdx];
        }
        if (tv.uvIdx < m_RawUVs.size()) {
            vert.uv = m_RawUVs[tv.uvIdx];
        }
        if (tv.normalIdx < m_RawNormals.size()) {
            vert.normal = m_RawNormals[tv.normalIdx];
        }

        m_Vertices.push_back(vert);
        m_Indices.push_back(static_cast<uint32_t>(i));
    }

    return true;
}

void MeshCanonicalizer::removeDuplicatesAndDegenerates() {
    std::vector<eng::renderer::PbrVertex> uniqueVerts;
    std::vector<uint32_t> uniqueIndices;
    
    // Hash map to locate duplicates
    struct PbrVertHash {
        std::size_t operator()(const eng::renderer::PbrVertex& v) const noexcept {
            return std::hash<float>{}(v.position.x) ^ 
                   (std::hash<float>{}(v.position.y) << 1) ^ 
                   (std::hash<float>{}(v.position.z) << 2);
        }
    };

    struct PbrVertEqual {
        bool operator()(const eng::renderer::PbrVertex& a, const eng::renderer::PbrVertex& b) const noexcept {
            return a.position == b.position && a.uv == b.uv && a.normal == b.normal;
        }
    };

    std::unordered_map<eng::renderer::PbrVertex, uint32_t, PbrVertHash, PbrVertEqual> vertToIndex;

    for (size_t i = 0; i < m_Indices.size(); i += 3) {
        uint32_t i0 = m_Indices[i + 0];
        uint32_t i1 = m_Indices[i + 1];
        uint32_t i2 = m_Indices[i + 2];

        const auto& v0 = m_Vertices[i0];
        const auto& v1 = m_Vertices[i1];
        const auto& v2 = m_Vertices[i2];

        // Filter degenerate (zero-area) triangles
        float area = glm::length(glm::cross(v1.position - v0.position, v2.position - v0.position));
        if (area < 1e-7f || i0 == i1 || i1 == i2 || i2 == i0) {
            continue;
        }

        auto processVert = [&](const eng::renderer::PbrVertex& v) {
            auto it = vertToIndex.find(v);
            if (it != vertToIndex.end()) {
                return it->second;
            }
            uint32_t newIdx = static_cast<uint32_t>(uniqueVerts.size());
            uniqueVerts.push_back(v);
            vertToIndex[v] = newIdx;
            return newIdx;
        };

        uniqueIndices.push_back(processVert(v0));
        uniqueIndices.push_back(processVert(v1));
        uniqueIndices.push_back(processVert(v2));
    }

    m_Vertices = std::move(uniqueVerts);
    m_Indices = std::move(uniqueIndices);
}

void MeshCanonicalizer::generateNormalsAndTangents() {
    // Generate missing flat normals
    std::vector<uint32_t> normalWriteCount(m_Vertices.size(), 0);
    for (size_t i = 0; i < m_Indices.size(); i += 3) {
        uint32_t i0 = m_Indices[i + 0];
        uint32_t i1 = m_Indices[i + 1];
        uint32_t i2 = m_Indices[i + 2];

        auto& v0 = m_Vertices[i0];
        auto& v1 = m_Vertices[i1];
        auto& v2 = m_Vertices[i2];

        if (glm::length(v0.normal) < 1e-4f || glm::length(v1.normal) < 1e-4f || glm::length(v2.normal) < 1e-4f) {
            glm::vec3 flatNormal = glm::normalize(glm::cross(v1.position - v0.position, v2.position - v0.position));
            if (glm::length(v0.normal) < 1e-4f) v0.normal = flatNormal;
            if (glm::length(v1.normal) < 1e-4f) v1.normal = flatNormal;
            if (glm::length(v2.normal) < 1e-4f) v2.normal = flatNormal;
        }
    }

    // Generate tangents
    std::vector<glm::vec3> accumulatedTangents(m_Vertices.size(), glm::vec3(0.0f));
    for (size_t i = 0; i < m_Indices.size(); i += 3) {
        uint32_t i0 = m_Indices[i + 0];
        uint32_t i1 = m_Indices[i + 1];
        uint32_t i2 = m_Indices[i + 2];

        const auto& v0 = m_Vertices[i0];
        const auto& v1 = m_Vertices[i1];
        const auto& v2 = m_Vertices[i2];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 deltaUV1 = v1.uv - v0.uv;
        glm::vec2 deltaUV2 = v2.uv - v0.uv;

        float det = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        glm::vec3 tangent(0.0f);
        if (std::abs(det) > 1e-6f) {
            float f = 1.0f / det;
            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            tangent = glm::normalize(tangent);
        } else {
            tangent = glm::normalize(edge1);
        }

        accumulatedTangents[i0] += tangent;
        accumulatedTangents[i1] += tangent;
        accumulatedTangents[i2] += tangent;
    }

    for (size_t i = 0; i < m_Vertices.size(); ++i) {
        auto& v = m_Vertices[i];
        glm::vec3 t = accumulatedTangents[i];
        if (glm::length(t) > 1e-4f) {
            t = glm::normalize(t);
            // Gram-Schmidt orthogonalization
            glm::vec3 normal = glm::normalize(v.normal);
            glm::vec3 orthoTangent = glm::normalize(t - normal * glm::dot(normal, t));
            v.tangent = glm::vec4(orthoTangent, 1.0f);
        } else {
            v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}

void MeshCanonicalizer::calculateBounds() {
    if (m_Vertices.empty()) return;

    glm::vec3 minB = m_Vertices[0].position;
    glm::vec3 maxB = m_Vertices[0].position;

    for (const auto& v : m_Vertices) {
        minB = glm::min(minB, v.position);
        maxB = glm::max(maxB, v.position);
    }

    std::cout << " - Bounds Min: (" << minB.x << ", " << minB.y << ", " << minB.z << ")\n"
              << " - Bounds Max: (" << maxB.x << ", " << maxB.y << ", " << maxB.z << ")\n";
}

} // namespace eng::cooker
