#include "ClusterGroupBuilder.h"
#include <unordered_map>
#include <unordered_set>
#include <map>

namespace eng::cooker {

bool ClusterGroupBuilder::GroupClusters(
    const std::vector<CookedCluster>& clusters,
    uint32_t maxGroupSize
) {
    m_Groups.clear();
    if (clusters.empty()) return true;

    // Group vertex positions to map sharing clusters
    struct Vec3Compare {
        bool operator()(const glm::vec3& a, const glm::vec3& b) const {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        }
    };

    std::map<glm::vec3, std::vector<uint32_t>, Vec3Compare> posToClusters;
    for (uint32_t i = 0; i < clusters.size(); ++i) {
        for (const auto& v : clusters[i].vertices) {
            posToClusters[v.position].push_back(i);
        }
    }

    // Map cluster adjacency by shared vertices count
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> adjacencyWeights;
    for (const auto& [pos, containingClusters] : posToClusters) {
        if (containingClusters.size() > 1) {
            for (size_t i = 0; i < containingClusters.size(); ++i) {
                for (size_t j = i + 1; j < containingClusters.size(); ++j) {
                    uint32_t c0 = containingClusters[i];
                    uint32_t c1 = containingClusters[j];
                    adjacencyWeights[c0][c1]++;
                    adjacencyWeights[c1][c0]++;
                }
            }
        }
    }

    std::unordered_set<uint32_t> unassignedClusters;
    for (uint32_t i = 0; i < clusters.size(); ++i) {
        unassignedClusters.insert(i);
    }

    while (!unassignedClusters.empty()) {
        uint32_t startCluster = *unassignedClusters.begin();
        unassignedClusters.erase(startCluster);

        ClusterGroup group;
        group.clusterIndices.push_back(startCluster);

        while (group.clusterIndices.size() < maxGroupSize) {
            uint32_t bestNeighbor = 0xFFFFFFFF;
            uint32_t bestWeight = 0;

            for (uint32_t c : group.clusterIndices) {
                auto it = adjacencyWeights.find(c);
                if (it != adjacencyWeights.end()) {
                    for (const auto& [neighbor, weight] : it->second) {
                        if (unassignedClusters.find(neighbor) != unassignedClusters.end()) {
                            if (weight > bestWeight) {
                                bestWeight = weight;
                                bestNeighbor = neighbor;
                            }
                        }
                    }
                }
            }

            if (bestNeighbor != 0xFFFFFFFF) {
                group.clusterIndices.push_back(bestNeighbor);
                unassignedClusters.erase(bestNeighbor);
            } else {
                break;
            }
        }

        m_Groups.push_back(std::move(group));
    }

    return true;
}

} // namespace eng::cooker
