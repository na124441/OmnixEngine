#include "ClusterSimplifier.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <map>

namespace eng::cooker {

struct PositionCompare {
    bool operator()(const glm::vec3& a, const glm::vec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};

struct SimpEdge {
    uint32_t v0 = 0;
    uint32_t v1 = 0;

    bool operator==(const SimpEdge& other) const {
        return (v0 == other.v0 && v1 == other.v1) || (v0 == other.v1 && v1 == other.v0);
    }
};

} // namespace eng::cooker

namespace std {
    template <>
    struct hash<eng::cooker::SimpEdge> {
        std::size_t operator()(const eng::cooker::SimpEdge& e) const noexcept {
            uint32_t low = e.v0 < e.v1 ? e.v0 : e.v1;
            uint32_t high = e.v0 < e.v1 ? e.v1 : e.v0;
            return std::hash<uint64_t>{}((static_cast<uint64_t>(low) << 32) | high);
        }
    };
}

namespace eng::cooker {

bool ClusterSimplifier::Simplify(
    const ClusterGroup& group,
    const std::vector<CookedCluster>& allClusters,
    std::vector<eng::renderer::PbrVertex>& outVertices,
    std::vector<uint32_t>& outIndices,
    float& outGeometricError,
    bool lockBoundaries
) {
    outVertices.clear();
    outIndices.clear();
    outGeometricError = 0.0f;

    std::vector<uint32_t> localToGlobalPosIdx;
    std::map<glm::vec3, uint32_t, PositionCompare> globalPosToIndex;
    auto getGlobalPosIdx = [&](const glm::vec3& pos) {
        auto it = globalPosToIndex.find(pos);
        if (it != globalPosToIndex.end()) return it->second;
        uint32_t idx = static_cast<uint32_t>(globalPosToIndex.size());
        globalPosToIndex[pos] = idx;
        return idx;
    };

    std::unordered_map<SimpEdge, std::unordered_set<uint32_t>, std::hash<SimpEdge>> globalEdgeToClusters;
    for (uint32_t cIdx = 0; cIdx < allClusters.size(); ++cIdx) {
        const auto& c = allClusters[cIdx];
        uint32_t triCount = static_cast<uint32_t>(c.indices.size() / 3);
        for (uint32_t i = 0; i < triCount; ++i) {
            uint32_t gp0 = getGlobalPosIdx(c.vertices[c.indices[i * 3 + 0]].position);
            uint32_t gp1 = getGlobalPosIdx(c.vertices[c.indices[i * 3 + 1]].position);
            uint32_t gp2 = getGlobalPosIdx(c.vertices[c.indices[i * 3 + 2]].position);

            globalEdgeToClusters[SimpEdge{gp0, gp1}].insert(cIdx);
            globalEdgeToClusters[SimpEdge{gp1, gp2}].insert(cIdx);
            globalEdgeToClusters[SimpEdge{gp2, gp0}].insert(cIdx);
        }
    }

    std::unordered_set<uint32_t> groupClusterIndices(group.clusterIndices.begin(), group.clusterIndices.end());
    for (uint32_t cIdx : group.clusterIndices) {
        const auto& c = allClusters[cIdx];
        uint32_t indexOffset = static_cast<uint32_t>(outVertices.size());
        for (const auto& v : c.vertices) {
            outVertices.push_back(v);
            localToGlobalPosIdx.push_back(getGlobalPosIdx(v.position));
        }
        for (uint32_t idx : c.indices) {
            outIndices.push_back(idx + indexOffset);
        }
    }

    if (outIndices.empty()) return true;

    // Detect and lock external boundaries
    std::vector<bool> lockedVertices(outVertices.size(), false);
    if (lockBoundaries) {
        for (size_t i = 0; i < outIndices.size(); i += 3) {
            uint32_t i0 = outIndices[i + 0];
            uint32_t i1 = outIndices[i + 1];
            uint32_t i2 = outIndices[i + 2];

            uint32_t gp0 = localToGlobalPosIdx[i0];
            uint32_t gp1 = localToGlobalPosIdx[i1];
            uint32_t gp2 = localToGlobalPosIdx[i2];

            SimpEdge edges[3] = { {gp0, gp1}, {gp1, gp2}, {gp2, gp0} };
            uint32_t localVerts[3] = { i0, i1, i2 };

            for (int e = 0; e < 3; ++e) {
                bool isExternal = false;
                auto it = globalEdgeToClusters.find(edges[e]);
                if (it != globalEdgeToClusters.end()) {
                    for (uint32_t sharingCluster : it->second) {
                        if (groupClusterIndices.find(sharingCluster) == groupClusterIndices.end()) {
                            isExternal = true;
                            break;
                        }
                    }
                }

                if (isExternal) {
                    lockedVertices[localVerts[e]] = true;
                    lockedVertices[localVerts[(e + 1) % 3]] = true;
                }
            }
        }
    }

    uint32_t targetTriangles = static_cast<uint32_t>((outIndices.size() / 3) / 2);
    if (targetTriangles < 1) targetTriangles = 1;

    std::vector<bool> vertexActive(outVertices.size(), true);
    bool collapsedAny = true;
    float maxCollapseError = 0.0f;

    while (collapsedAny && (outIndices.size() / 3) > targetTriangles) {
        collapsedAny = false;

        for (size_t i = 0; i < outIndices.size(); i += 3) {
            uint32_t i0 = outIndices[i + 0];
            uint32_t i1 = outIndices[i + 1];
            uint32_t i2 = outIndices[i + 2];

            uint32_t pairs[3][2] = { {i0, i1}, {i1, i2}, {i2, i0} };
            for (int p = 0; p < 3; ++p) {
                uint32_t u = pairs[p][0];
                uint32_t v = pairs[p][1];

                if (!vertexActive[u] || !vertexActive[v]) continue;
                if (lockedVertices[u] || lockedVertices[v]) continue;

                float edgeLen = glm::distance(outVertices[u].position, outVertices[v].position);
                maxCollapseError = glm::max(maxCollapseError, edgeLen);

                vertexActive[v] = false;
                for (auto& idx : outIndices) {
                    if (idx == v) {
                        idx = u;
                    }
                }

                collapsedAny = true;
                break;
            }

            if (collapsedAny) break;
        }

        std::vector<uint32_t> cleanIndices;
        cleanIndices.reserve(outIndices.size());
        for (size_t i = 0; i < outIndices.size(); i += 3) {
            uint32_t i0 = outIndices[i + 0];
            uint32_t i1 = outIndices[i + 1];
            uint32_t i2 = outIndices[i + 2];

            if (i0 != i1 && i1 != i2 && i2 != i0) {
                cleanIndices.push_back(i0);
                cleanIndices.push_back(i1);
                cleanIndices.push_back(i2);
            }
        }
        outIndices = std::move(cleanIndices);
    }

    outGeometricError = maxCollapseError;

    std::vector<eng::renderer::PbrVertex> compactedVerts;
    std::vector<uint32_t> indexMapping(outVertices.size(), 0xFFFFFFFF);
    for (uint32_t idx : outIndices) {
        if (indexMapping[idx] == 0xFFFFFFFF) {
            indexMapping[idx] = static_cast<uint32_t>(compactedVerts.size());
            compactedVerts.push_back(outVertices[idx]);
        }
    }

    for (auto& idx : outIndices) {
        idx = indexMapping[idx];
    }
    outVertices = std::move(compactedVerts);

    return true;
}

} // namespace eng::cooker
