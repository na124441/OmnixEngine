#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "RenderObject.h"
#include "ModelLoader.h"

namespace eng::renderer {

#ifndef OMNIX_LEGACY_RENDER_SCENE_CLASS
#define OMNIX_LEGACY_RENDER_SCENE_CLASS
class RenderSceneCache {
public:
    RenderSceneCache() = default;
    ~RenderSceneCache() = default;

    // -----------------------------------------------------------------
    // Mesh management
    Mesh* createMesh()
    {
        meshes.emplace_back(std::make_unique<Mesh>());
        return meshes.back().get();
    }
    Mesh* createMeshFromOBJ(const std::string& path, EngineResources& resources);
    Mesh* createMeshFromOmnixMesh(const std::string& path, EngineResources& resources);
    Mesh* createMeshFromRVG(const std::string& path, EngineResources& resources);
    void destroyMesh(Mesh* m)
    {
        auto it = std::find_if(meshes.begin(), meshes.end(),
                               [&](const std::unique_ptr<Mesh>& p){ return p.get() == m; });
        if (it != meshes.end()) {
            (*it)->destroy();
            meshes.erase(it);
        }
    }

    // -----------------------------------------------------------------
    // Material management (non‑owning – pipeline life‑time lives elsewhere)
    Material* createMaterial()
    {
        materials.emplace_back(std::make_unique<Material>());
        return materials.back().get();
    }
    void destroyMaterial(Material* m)
    {
        auto it = std::find_if(materials.begin(), materials.end(),
                               [&](const std::unique_ptr<Material>& p){ return p.get() == m; });
        if (it != materials.end()) {
            materials.erase(it);
        }
    }

    // -----------------------------------------------------------------
    // Object management
    RenderObject& addObject(Mesh* mesh, Material* mat, const glm::mat4& transform = glm::mat4(1.0f))
    {
        RenderObject ro{};
        ro.mesh      = mesh;
        ro.material  = mat;
        ro.transform = transform;
        objects.emplace_back(ro);
        return objects.back();
    }

    void clearObjects() { objects.clear(); }
    void clearAll() {
        for (auto& m : meshes) { if (m) m->destroy(); }
        meshes.clear();
        for (auto& mat : materials) { if (mat) mat->destroy(); }
        materials.clear();
        objects.clear();
    }

    // -----------------------------------------------------------------
    // Accessors
    const std::vector<RenderObject>& getObjects() const { return objects; }

private:
    std::vector<std::unique_ptr<Mesh>>     meshes;
    std::vector<std::unique_ptr<Material>>materials;
    std::vector<RenderObject>               objects;
};
#endif

} // namespace eng::renderer
