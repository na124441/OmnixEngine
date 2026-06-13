#include "Core/pch.h"
#include "Scene.h"
#include "ModelLoader.h"
#include "Core/Engine/EngineResources.h"
#include "Runtime/Public/OmnixMeshFormat.h"

namespace eng::renderer {

Mesh* RenderSceneCache::createMeshFromOBJ(const std::string& path, EngineResources& resources)
{
    Mesh* m = createMesh();
    if (!ModelLoader::LoadOBJ(path, *m, resources)) {
        destroyMesh(m);
        return nullptr;
    }
    return m;
}

Mesh* RenderSceneCache::createMeshFromOmnixMesh(const std::string& path, EngineResources& resources)
{
    OmnixMesh meshData;
    if (!DeserializeMesh(meshData, path)) {
        ::Logger::Log(::LogLevel::Error, "Failed to deserialize .omnixmesh: " + path);
        return nullptr;
    }

    std::vector<PbrVertex> vertices(meshData.header.vertexCount);
    for (size_t i = 0; i < meshData.header.vertexCount; ++i) {
        vertices[i].pos = glm::vec3(meshData.vertices[i].position.x, meshData.vertices[i].position.y, meshData.vertices[i].position.z);
        vertices[i].normal = glm::vec3(meshData.vertices[i].normal.x, meshData.vertices[i].normal.y, meshData.vertices[i].normal.z);
        vertices[i].uv = glm::vec2(meshData.vertices[i].uv0.x, meshData.vertices[i].uv0.y);
    }

    Mesh* m = createMesh();
    m->minBounds = glm::vec3(meshData.header.bounds.min.x, meshData.header.bounds.min.y, meshData.header.bounds.min.z);
    m->maxBounds = glm::vec3(meshData.header.bounds.max.x, meshData.header.bounds.max.y, meshData.header.bounds.max.z);

    if (!m->init(vertices.data(), vertices.size(), meshData.indices.data(), meshData.indices.size(), resources)) {
        destroyMesh(m);
        return nullptr;
    }
    return m;
}

} // namespace eng::renderer
