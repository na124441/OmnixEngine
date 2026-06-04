#pragma once

#include "tiny_gltf.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"

#include "renderer/scene/Mesh.h"
#include "renderer/scene/Material.h"
#include "renderer/scene/Texture.h"
#include "renderer/scene/Scene.h"
#include "Core/Engine/EngineResources.h"
#include "Core/types/Vertex.h"

namespace eng::renderer {

/**
 * Loads a GLTF/GLB file, creates Vulkan meshes, textures and PBR
 * materials, and inserts the resulting RenderObjects into a Scene.
 */
class GltfModel
{
public:
    GltfModel() = default;
    ~GltfModel() = default;

    GltfModel(const GltfModel&) = delete;
    GltfModel& operator=(const GltfModel&) = delete;

    /**
     * @param filename   Path to a `.gltf` or `.glb` file.
     * @param resources  Global Vulkan resources (device, allocator, pools…).
     * @param scene      The scene that will own the created meshes/materials.
     * @return true on success, false on any error.
     */
    bool load(const std::string& filename,
              EngineResources& resources,
              RenderScene& scene);

private:
    // Load a single GLTF image (by its index) into a Texture.
    std::shared_ptr<Texture> loadTexture(int imageIndex,
                                          const tinygltf::Model& model,
                                          EngineResources& resources);

    // Build a PBR Material from a GLTF material description.
    std::unique_ptr<Material> createMaterial(const tinygltf::Material& gltfMat,
                                             const tinygltf::Model& model,
                                             EngineResources& resources);

    // Convert GLTF primitive attributes into a flat vertex array.
    static std::vector<PbrVertex> buildVertices(const tinygltf::Primitive& prim,
                                                const tinygltf::Model& model);

    // Convert GLTF index accessor into a flat uint32_t vector.
    static std::vector<uint32_t> buildIndices(const tinygltf::Primitive& prim,
                                              const tinygltf::Model& model);
};

} // namespace eng::renderer
