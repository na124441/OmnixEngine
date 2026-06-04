#include "Core/pch.h"
#include "stb/stb_image_write.h"
#include "GltfModel.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/ResourceTracker.h"
#include "Core/Engine/VmaHelpers.h"
#include <cstring>
#include <cassert>
#include <iostream>
#include <cstdio> // for std::remove

namespace eng::renderer {

// ---------------------------------------------------------------------
// Load a GLTF image → temporary PNG → our Texture class.
std::shared_ptr<Texture> GltfModel::loadTexture(int imageIndex,
                                                const tinygltf::Model& model,
                                                EngineResources& resources)
{
    // Validate image index
    if (imageIndex < 0 || imageIndex >= static_cast<int>(model.images.size())) {
        LOG_WARN("GLTF: texture index out of range – using white fallback");
        return std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

    const tinygltf::Image& img = model.images[imageIndex];

    // Write the raw image data to a temporary PNG file.
    std::string tmpPath = "tmp_gltf_img_" + std::to_string(imageIndex) + ".png";

    const unsigned char* pixels = img.image.data();

    if (!stbi_write_png(tmpPath.c_str(),
                        img.width,
                        img.height,
                        img.component,
                        pixels,
                        img.width * img.component)) {
        LOG_ERROR("Failed to write temporary PNG for GLTF image");
        return std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

    // Load the PNG via our existing Texture::loadFromFile() method.
    auto tex = std::make_shared<Texture>();
    if (!tex->loadFromFile(tmpPath,
                           resources.device,
                           resources.allocator,
                           resources.commandPools[0],
                           resources.graphicsQueue)) {
        LOG_ERROR("Failed to load texture from temporary PNG: " + tmpPath);
        std::remove(tmpPath.c_str());
        return std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

    // Cleanup temp file
    std::remove(tmpPath.c_str());
return tex;
}

// ---------------------------------------------------------------------
// Convert a GLTF primitive into our simple PbrVertex array.
std::vector<PbrVertex> GltfModel::buildVertices(const tinygltf::Primitive& prim,
                                                const tinygltf::Model& model)
{
    auto readAttribute = [&](const std::string& name,
                             std::vector<float>& out,
                             int& componentCount) -> bool
    {
        auto it = prim.attributes.find(name);
        if (it == prim.attributes.end())
            return false; // attribute not present

        const tinygltf::Accessor& accessor = model.accessors[it->second];
        const tinygltf::BufferView& view   = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer&     buffer = model.buffers[view.buffer];

        const size_t stride = accessor.ByteStride(view);
        const size_t count  = accessor.count;
        componentCount = accessor.type == TINYGLTF_TYPE_VEC3 ? 3 :
                        accessor.type == TINYGLTF_TYPE_VEC2 ? 2 : 1;

        out.resize(count * componentCount);
        const unsigned char* src = buffer.data.data() + accessor.byteOffset + view.byteOffset;
        for (size_t i = 0; i < count; ++i) {
            const unsigned char* elem = src + i * stride;
            std::memcpy(&out[i * componentCount], elem, componentCount * sizeof(float));
        }
        return true;
    };

    std::vector<float> posData; int posComp = 0;
    bool ok = readAttribute("POSITION", posData, posComp);
    assert(ok && posComp == 3);

    std::vector<float> normData; int normComp = 0;
    readAttribute("NORMAL", normData, normComp);

    std::vector<float> uvData; int uvComp = 0;
    readAttribute("TEXCOORD_0", uvData, uvComp);

    size_t vertexCount = posData.size() / 3;
    std::vector<PbrVertex> verts;
    verts.reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        PbrVertex v{};
        v.pos = glm::vec3(posData[3*i+0], posData[3*i+1], posData[3*i+2]);

        if (!normData.empty()) {
            v.normal = glm::vec3(normData[3*i+0], normData[3*i+1], normData[3*i+2]);
        } else {
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        if (!uvData.empty()) {
            v.uv = glm::vec2(uvData[2*i+0], uvData[2*i+1]);
        } else {
            v.uv = glm::vec2(0.0f, 0.0f);
        }
        verts.push_back(v);
    }
    return verts;
}

// ---------------------------------------------------------------------
std::vector<uint32_t> GltfModel::buildIndices(const tinygltf::Primitive& prim,
                                              const tinygltf::Model& model)
{
    if (prim.indices < 0) return {};

    const tinygltf::Accessor& acc   = model.accessors[prim.indices];
    const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];

    const unsigned char* src = buffer.data.data() + acc.byteOffset + view.byteOffset;
    std::vector<uint32_t> indices;
    indices.reserve(acc.count);

    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        for (size_t i = 0; i < acc.count; ++i) {
            uint16_t idx;
            std::memcpy(&idx, src + i * 2, sizeof(uint16_t));
            indices.push_back(static_cast<uint32_t>(idx));
        }
    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        for (size_t i = 0; i < acc.count; ++i) {
            uint32_t idx;
            std::memcpy(&idx, src + i * 4, sizeof(uint32_t));
            indices.push_back(idx);
        }
    } else {
        LOG_ERROR("GLTF: Unsupported index component type");
    }
    return indices;
}

// ---------------------------------------------------------------------
std::unique_ptr<Material> GltfModel::createMaterial(const tinygltf::Material& gltfMat,
                                                   const tinygltf::Model& model,
                                                   EngineResources& resources)
{
    auto mat = std::make_unique<Material>();

    // Albedo
    if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
        int imgIdx = model.textures[gltfMat.pbrMetallicRoughness.baseColorTexture.index].source;
        mat->albedoTexture = loadTexture(imgIdx, model, resources);
    } else {
        mat->albedoTexture = std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

    // Normal
    if (gltfMat.normalTexture.index >= 0) {
        int imgIdx = model.textures[gltfMat.normalTexture.index].source;
        mat->normalTexture = loadTexture(imgIdx, model, resources);
    } else {
        mat->normalTexture = std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

    mat->uboData.metallic  = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor);
    mat->uboData.roughness = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor);
    mat->dirty = true;

    // Create the graphics pipeline
    bool ok = mat->create("shaders/pbr_vert.spv",
                         "shaders/pbr_frag.spv",
                         "", "",
                         resources);
    if (!ok) {
        LOG_ERROR("Failed to create Material for GLTF material");
        return nullptr;
    }
    return mat;
}

// ---------------------------------------------------------------------
bool GltfModel::load(const std::string& filename,
                     EngineResources& resources,
                     RenderScene& scene)
{
    tinygltf::Model   model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    if (!ok) {
        // Try binary if ASCII fails
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
        if (!ok) {
            LOG_ERROR("Failed to load GLTF file '" + filename + "': " + err);
            return false;
        }
    }
    if (!warn.empty()) {
        LOG_WARN("GLTF warnings: " + warn);
    }

    LOG_INFO("GLTF model '" + filename + "' – " +
             std::to_string(model.meshes.size()) + " mesh(es), " +
             std::to_string(model.materials.size()) + " material(s).");

    // Build a cache of Materials in the scene
    std::vector<Material*> matCache;
    matCache.reserve(model.materials.size());

    for (size_t i = 0; i < model.materials.size(); ++i) {
        const tinygltf::Material& gltfMat = model.materials[i];
        auto mat = createMaterial(gltfMat, model, resources);
        if (!mat) {
            LOG_ERROR("Failed to create material for GLTF material index " + std::to_string(i));
            return false;
        }
        // Move to scene ownership
        Material* sceneMat = scene.createMaterial();
        *sceneMat = std::move(*mat);
        matCache.push_back(sceneMat);
    }

    // Default material
    Material* defaultMat = nullptr;

    for (const tinygltf::Mesh& gltfMesh : model.meshes) {
        for (const tinygltf::Primitive& prim : gltfMesh.primitives) {
            std::vector<PbrVertex> vertices = buildVertices(prim, model);
            std::vector<uint32_t>   indices  = buildIndices (prim, model);
            if (vertices.empty()) {
                LOG_WARN("GLTF primitive has empty vertex data – skipping.");
                continue;
            }

            Mesh* mesh = scene.createMesh();
            bool meshOk = mesh->init(vertices.data(), vertices.size(),
                                    indices.data(),  indices.size(),
                                    resources);
            if (!meshOk) {
                LOG_ERROR("Failed to initialise Mesh for GLTF primitive");
                return false;
            }

            Material* mat = nullptr;
            if (prim.material >= 0 && prim.material < static_cast<int>(matCache.size())) {
                mat = matCache[prim.material];
            } else {
                if (!defaultMat) {
                    defaultMat = scene.createMaterial();
                    defaultMat->albedoTexture = std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
                    defaultMat->normalTexture = std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
                    defaultMat->uboData.metallic  = 0.0f;
                    defaultMat->uboData.roughness = 0.5f;
                    defaultMat->dirty = true;
                    defaultMat->create("shaders/pbr_vert.spv",
                                      "shaders/pbr_frag.spv",
                                      "", "",
                                      resources);
                }
                mat = defaultMat;
            }

            scene.addObject(mesh, mat, glm::mat4(1.0f));
        }
    }

    LOG_INFO("GLTF model conversion finished – scene now has "
              + std::to_string(scene.getObjects().size()) + " render objects.");
    return true;
}

} // namespace eng::renderer
