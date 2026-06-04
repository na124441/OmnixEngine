#include "Core/pch.h"
#include "Scene.h"
#include "ModelLoader.h"
#include "Core/Engine/EngineResources.h"

namespace eng::renderer {

Mesh* RenderScene::createMeshFromOBJ(const std::string& path, EngineResources& resources)
{
    Mesh* m = createMesh();
    if (!ModelLoader::LoadOBJ(path, *m, resources)) {
        destroyMesh(m);
        return nullptr;
    }
    return m;
}

} // namespace eng::renderer
