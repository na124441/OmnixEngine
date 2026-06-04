#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "RenderObject.h"
#include "ModelLoader.h"

namespace eng::renderer {

/// A lightweight container that owns meshes & materials and
/// provides a simple API for adding/removing objects.
class RenderScene {
public:
    RenderScene() = default;
    ~RenderScene() = default;

    // -----------------------------------------------------------------
    // Mesh management
    Mesh* createMesh()
    {
        meshes.emplace_back(std::make_unique<Mesh>());
        return meshes.back().get();
    }
    Mesh* createMeshFromOBJ(const std::string& path, EngineResources& resources);
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

    // -----------------------------------------------------------------
    // Accessors
    const std::vector<RenderObject>& getObjects() const { return objects; }

private:
    std::vector<std::unique_ptr<Mesh>>     meshes;
    std::vector<std::unique_ptr<Material>>materials;
    std::vector<RenderObject>               objects;
};

} // namespace eng::renderer
