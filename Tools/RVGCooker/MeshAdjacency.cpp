#include "MeshAdjacency.h"
#include <iostream>
#include <map>

namespace eng::cooker {

bool MeshAdjacency::BuildAdjacency(const std::vector<eng::renderer::PbrVertex>& vertices, const std::vector<uint32_t>& indices) {
    if (indices.size() % 3 != 0) {
        return false;
    }

    std::unordered_map<Edge, std::vector<uint32_t>> edgeToTriangles;
    uint32_t triangleCount = static_cast<uint32_t>(indices.size() / 3);

    for (uint32_t i = 0; i < triangleCount; ++i) {
        uint32_t v0 = indices[i * 3 + 0];
        uint32_t v1 = indices[i * 3 + 1];
        uint32_t v2 = indices[i * 3 + 2];

        edgeToTriangles[Edge{v0, v1}].push_back(i);
        edgeToTriangles[Edge{v1, v2}].push_back(i);
        edgeToTriangles[Edge{v2, v0}].push_back(i);
    }

    m_BoundaryEdgeCount = 0;
    m_NonManifoldEdgeCount = 0;

    for (const auto& [edge, triangles] : edgeToTriangles) {
        if (triangles.size() == 1) {
            m_BoundaryEdgeCount++;
        } else if (triangles.size() > 2) {
            m_NonManifoldEdgeCount++;
        }
    }

    // Detect UV and normal seams
    m_UvSeamCount = 0;
    m_NormalSeamCount = 0;

    // Group vertices by position to check seams
    struct Vec3Compare {
        bool operator()(const glm::vec3& a, const glm::vec3& b) const {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        }
    };

    std::map<glm::vec3, std::vector<eng::renderer::PbrVertex>, Vec3Compare> posGroups;
    for (const auto& v : vertices) {
        posGroups[v.position].push_back(v);
    }

    for (const auto& [pos, group] : posGroups) {
        if (group.size() > 1) {
            bool hasUvDiff = false;
            bool hasNormalDiff = false;
            const auto& first = group[0];
            for (size_t i = 1; i < group.size(); ++i) {
                if (group[i].uv != first.uv) {
                    hasUvDiff = true;
                }
                if (group[i].normal != first.normal) {
                    hasNormalDiff = true;
                }
            }
            if (hasUvDiff) m_UvSeamCount++;
            if (hasNormalDiff) m_NormalSeamCount++;
        }
    }

    return true;
}

void MeshAdjacency::PrintReport() const {
    std::cout << "--- Adjacency Analysis Report ---\n"
              << " - Boundary Edges:     " << m_BoundaryEdgeCount << "\n"
              << " - Non-Manifold Edges: " << m_NonManifoldEdgeCount << "\n"
              << " - UV Seams:           " << m_UvSeamCount << "\n"
              << " - Hard-Normal Seams:  " << m_NormalSeamCount << "\n"
              << "---------------------------------\n";
}

} // namespace eng::cooker
