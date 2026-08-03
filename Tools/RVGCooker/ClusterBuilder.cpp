#include "ClusterBuilder.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace eng::cooker {

struct AdjacencyEdge {
    uint32_t v0 = 0;
    uint32_t v1 = 0;

    bool operator==(const AdjacencyEdge& other) const {
        return (v0 == other.v0 && v1 == other.v1) || (v0 == other.v1 && v1 == other.v0);
    }
};

struct AdjacencyEdgeHash {
    std::size_t operator()(const AdjacencyEdge& e) const noexcept {
        uint32_t low = e.v0 < e.v1 ? e.v0 : e.v1;
        uint32_t high = e.v0 < e.v1 ? e.v1 : e.v0;
        return std::hash<uint64_t>{}((static_cast<uint64_t>(low) << 32) | high);
    }
};

bool ClusterBuilder::BuildClusters(
    const std::vector<eng::renderer::PbrVertex>& vertices,
    const std::vector<uint32_t>& indices,
    uint32_t vertexLimit,
    uint32_t triangleLimit
) {
    m_Clusters.clear();
    if (indices.empty()) return true;

    // Phase 1: Material partitioning (default to 0 if not specified)
    // Group raw triangle indices by material. Since raw indices are a flat list, we process them as list of triangles.
    uint32_t rawTriangleCount = static_cast<uint32_t>(indices.size() / 3);
    std::vector<uint32_t> partitionTriangles;
    partitionTriangles.reserve(rawTriangleCount);
    for (uint32_t i = 0; i < rawTriangleCount; ++i) {
        partitionTriangles.push_back(i);
    }

    // Phase 2: Adjacency map construction for cluster growth
    std::unordered_map<AdjacencyEdge, std::vector<uint32_t>, AdjacencyEdgeHash> edgeToTriangles;
    for (uint32_t triIdx : partitionTriangles) {
        uint32_t v0 = indices[triIdx * 3 + 0];
        uint32_t v1 = indices[triIdx * 3 + 1];
        uint32_t v2 = indices[triIdx * 3 + 2];

        edgeToTriangles[AdjacencyEdge{v0, v1}].push_back(triIdx);
        edgeToTriangles[AdjacencyEdge{v1, v2}].push_back(triIdx);
        edgeToTriangles[AdjacencyEdge{v2, v0}].push_back(triIdx);
    }

    std::unordered_set<uint32_t> unassignedTriangles(partitionTriangles.begin(), partitionTriangles.end());

    // Phase 3: Cluster Growth
    while (!unassignedTriangles.empty()) {
        // Pick seed triangle
        uint32_t seedTriIdx = *unassignedTriangles.begin();
        unassignedTriangles.erase(seedTriIdx);

        std::vector<uint32_t> clusterTriangles;
        clusterTriangles.push_back(seedTriIdx);

        std::unordered_set<uint32_t> clusterVertices;
        clusterVertices.insert(indices[seedTriIdx * 3 + 0]);
        clusterVertices.insert(indices[seedTriIdx * 3 + 1]);
        clusterVertices.insert(indices[seedTriIdx * 3 + 2]);

        // Breadth-First-Search queue for adjacent candidate triangles
        std::queue<uint32_t> candidateQueue;
        
        auto enqueueNeighbors = [&](uint32_t tri) {
            uint32_t v0 = indices[tri * 3 + 0];
            uint32_t v1 = indices[tri * 3 + 1];
            uint32_t v2 = indices[tri * 3 + 2];

            AdjacencyEdge edges[3] = { {v0, v1}, {v1, v2}, {v2, v0} };
            for (const auto& edge : edges) {
                auto it = edgeToTriangles.find(edge);
                if (it != edgeToTriangles.end()) {
                    for (uint32_t neighbor : it->second) {
                        if (unassignedTriangles.find(neighbor) != unassignedTriangles.end()) {
                            candidateQueue.push(neighbor);
                        }
                    }
                }
            }
        };

        enqueueNeighbors(seedTriIdx);

        while (!candidateQueue.empty() && clusterTriangles.size() < triangleLimit) {
            uint32_t candidate = candidateQueue.front();
            candidateQueue.pop();

            if (unassignedTriangles.find(candidate) == unassignedTriangles.end()) {
                continue; // Already processed or added
            }

            uint32_t v0 = indices[candidate * 3 + 0];
            uint32_t v1 = indices[candidate * 3 + 1];
            uint32_t v2 = indices[candidate * 3 + 2];

            // Count how many new vertices this candidate would add
            uint32_t newVertCount = 0;
            if (clusterVertices.find(v0) == clusterVertices.end()) newVertCount++;
            if (clusterVertices.find(v1) == clusterVertices.end()) newVertCount++;
            if (clusterVertices.find(v2) == clusterVertices.end()) newVertCount++;

            if (clusterVertices.size() + newVertCount <= vertexLimit) {
                clusterTriangles.push_back(candidate);
                clusterVertices.insert(v0);
                clusterVertices.insert(v1);
                clusterVertices.insert(v2);
                unassignedTriangles.erase(candidate);
                enqueueNeighbors(candidate);
            }
        }

        // Phase 4: Local Reindexing and Compaction
        CookedCluster cluster;
        cluster.materialIndex = 0; // Default material
        cluster.sourceTriangleIndices = clusterTriangles;

        // Map global vertex index to cluster-local vertex index
        std::unordered_map<uint32_t, uint32_t> globalToLocalIndex;
        for (uint32_t globalIdx : clusterVertices) {
            uint32_t localIdx = static_cast<uint32_t>(cluster.vertices.size());
            cluster.vertices.push_back(vertices[globalIdx]);
            globalToLocalIndex[globalIdx] = localIdx;
        }

        for (uint32_t triIdx : clusterTriangles) {
            cluster.indices.push_back(globalToLocalIndex[indices[triIdx * 3 + 0]]);
            cluster.indices.push_back(globalToLocalIndex[indices[triIdx * 3 + 1]]);
            cluster.indices.push_back(globalToLocalIndex[indices[triIdx * 3 + 2]]);
        }

        // Phase 5: Compute bounds & normal cone
        computeClusterBounds(cluster);
        computeNormalCone(cluster);

        m_Clusters.push_back(std::move(cluster));
    }

    // Verify lossless culling
    uint32_t cookedTriangles = 0;
    for (const auto& c : m_Clusters) {
        cookedTriangles += static_cast<uint32_t>(c.indices.size() / 3);
    }
    if (cookedTriangles != rawTriangleCount) {
        std::cerr << "Lossy Build Error: Expected " << rawTriangleCount << " triangles, but cooked " << cookedTriangles << ".\n";
        return false;
    }

    return true;
}

void ClusterBuilder::computeClusterBounds(CookedCluster& cluster) {
    if (cluster.vertices.empty()) return;

    cluster.boundsMin = cluster.vertices[0].position;
    cluster.boundsMax = cluster.vertices[0].position;

    for (const auto& v : cluster.vertices) {
        cluster.boundsMin = glm::min(cluster.boundsMin, v.position);
        cluster.boundsMax = glm::max(cluster.boundsMax, v.position);
    }

    // Bounding sphere
    glm::vec3 center = (cluster.boundsMin + cluster.boundsMax) * 0.5f;
    float maxDistSq = 0.0f;
    for (const auto& v : cluster.vertices) {
        glm::vec3 diff = v.position - center;
        maxDistSq = glm::max(maxDistSq, glm::dot(diff, diff));
    }
    cluster.boundsSphere = glm::vec4(center, std::sqrt(maxDistSq));
}

void ClusterBuilder::computeNormalCone(CookedCluster& cluster) {
    if (cluster.indices.empty()) return;

    // Gather face normals of all triangles inside this cluster
    std::vector<glm::vec3> faceNormals;
    uint32_t triangleCount = static_cast<uint32_t>(cluster.indices.size() / 3);
    glm::vec3 averageNormal(0.0f);

    for (uint32_t i = 0; i < triangleCount; ++i) {
        glm::vec3 p0 = cluster.vertices[cluster.indices[i * 3 + 0]].position;
        glm::vec3 p1 = cluster.vertices[cluster.indices[i * 3 + 1]].position;
        glm::vec3 p2 = cluster.vertices[cluster.indices[i * 3 + 2]].position;

        glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
        if (glm::length(normal) > 1e-6f) {
            normal = glm::normalize(normal);
            faceNormals.push_back(normal);
            averageNormal += normal;
        }
    }

    if (faceNormals.empty()) {
        cluster.coneAxis = glm::vec3(0.0f, 1.0f, 0.0f);
        cluster.coneCutoff = 1.0f;
        return;
    }

    if (glm::length(averageNormal) > 1e-4f) {
        cluster.coneAxis = glm::normalize(averageNormal);
    } else {
        cluster.coneAxis = faceNormals[0];
    }

    // Find the maximum deviation angle from average axis to define cutoff semi-angle
    float minCosTheta = 1.0f;
    for (const auto& normal : faceNormals) {
        float cosTheta = glm::dot(cluster.coneAxis, normal);
        minCosTheta = glm::min(minCosTheta, cosTheta);
    }

    cluster.coneCutoff = minCosTheta;
}

} // namespace eng::cooker
