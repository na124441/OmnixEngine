#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "RenderingEngine/Core/types/Vertex.h"

namespace eng::cooker {

struct Edge {
    uint32_t v0 = 0;
    uint32_t v1 = 0;

    bool operator==(const Edge& other) const {
        return (v0 == other.v0 && v1 == other.v1) || (v0 == other.v1 && v1 == other.v0);
    }
};

} // namespace eng::cooker

namespace std {
    template <>
    struct hash<eng::cooker::Edge> {
        std::size_t operator()(const eng::cooker::Edge& e) const noexcept {
            uint32_t low = e.v0 < e.v1 ? e.v0 : e.v1;
            uint32_t high = e.v0 < e.v1 ? e.v1 : e.v0;
            return std::hash<uint64_t>{}((static_cast<uint64_t>(low) << 32) | high);
        }
    };
}

namespace eng::cooker {

class MeshAdjacency {
public:
    MeshAdjacency() = default;
    ~MeshAdjacency() = default;

    bool BuildAdjacency(const std::vector<eng::renderer::PbrVertex>& vertices, const std::vector<uint32_t>& indices);
    void PrintReport() const;

    uint32_t GetBoundaryEdgeCount() const { return m_BoundaryEdgeCount; }
    uint32_t GetNonManifoldEdgeCount() const { return m_NonManifoldEdgeCount; }
    uint32_t GetNormalSeamCount() const { return m_NormalSeamCount; }
    uint32_t GetUvSeamCount() const { return m_UvSeamCount; }

private:
    uint32_t m_BoundaryEdgeCount = 0;
    uint32_t m_NonManifoldEdgeCount = 0;
    uint32_t m_NormalSeamCount = 0;
    uint32_t m_UvSeamCount = 0;
};

} // namespace eng::cooker
