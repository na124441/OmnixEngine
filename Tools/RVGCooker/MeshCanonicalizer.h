#pragma once
#include <string>
#include <vector>
#include "RenderingEngine/Core/types/Vertex.h"

namespace eng::cooker {

class MeshCanonicalizer {
public:
    MeshCanonicalizer() = default;
    ~MeshCanonicalizer() = default;

    bool Canonicalize(const std::string& sourcePath, const std::string& outputPath, uint32_t pageSize = 4096, uint32_t vertexLimit = 64, uint32_t triangleLimit = 80);

    const std::vector<eng::renderer::PbrVertex>& GetVertices() const { return m_Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

private:
    bool loadOBJ(const std::string& path);
    void removeDuplicatesAndDegenerates();
    void generateNormalsAndTangents();
    void calculateBounds();

    std::vector<eng::renderer::PbrVertex> m_Vertices;
    std::vector<uint32_t> m_Indices;

    // Raw components parsed from file
    std::vector<glm::vec3> m_RawPositions;
    std::vector<glm::vec3> m_RawNormals;
    std::vector<glm::vec2> m_RawUVs;
};

} // namespace eng::cooker
