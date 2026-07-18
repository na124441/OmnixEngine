#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "src/renderer/scene/RenderObject.h"

/// A lightweight container that owns meshes & materials and
/// provides a simple API for adding/removing objects.
class Scene {
public:
    Scene() = default;
    ~Scene() = default; // meshes/materials are destroyed via their own destructors.

    // -----------------------------------------------------------------
    // Mesh management
    Mesh* createMesh()
    {
        meshes.emplace_back(std::make_unique<Mesh>());
        return meshes.back().get();
    }
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
    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout)
    {
        materials.emplace_back(std::make_unique<Material>(pipeline, layout));
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
