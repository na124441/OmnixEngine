#include "Core/pch.h"
#include "GltfModel.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaHelpers.h"
#include <cstring>
#include <cassert>
#include <algorithm>
#include <iostream>

namespace eng::renderer {

// ---------------------------------------------------------------------
// Load a GLTF image directly into GPU memory.
std::shared_ptr<Texture> GltfModel::loadTexture(int imageIndex,
                                                const tinygltf::Model& model,
                                                EngineResources& resources,
                                                TextureUsage usage)
{
    // Validate image index
    if (imageIndex < 0 || imageIndex >= static_cast<int>(model.images.size())) {
        LOG_WARN("GLTF: texture index out of range – using white fallback");
        return std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

    const tinygltf::Image& img = model.images[imageIndex];
    if (img.width <= 0 || img.height <= 0 || img.image.empty()) {
        LOG_ERROR("GLTF: embedded image is empty or invalid; using fallback texture");
        return std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

    auto tex = std::make_shared<Texture>();
    std::vector<unsigned char> rgba(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4u, 255u);
    const int components = std::max(1, img.component);
    const unsigned char* src = img.image.data();

    for (size_t i = 0; i < static_cast<size_t>(img.width) * static_cast<size_t>(img.height); ++i) {
        const size_t srcIndex = i * static_cast<size_t>(components);
        const size_t dstIndex = i * 4u;
        const unsigned char r = src[srcIndex + 0];
        const unsigned char g = components > 1 ? src[srcIndex + 1] : r;
        const unsigned char b = components > 2 ? src[srcIndex + 2] : r;
        const unsigned char a = components > 3 ? src[srcIndex + 3] : 255u;
        rgba[dstIndex + 0] = r;
        rgba[dstIndex + 1] = g;
        rgba[dstIndex + 2] = b;
        rgba[dstIndex + 3] = a;
    }

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    if (usage == TextureUsage::Albedo || usage == TextureUsage::Emissive || usage == TextureUsage::UI) {
        format = VK_FORMAT_R8G8B8A8_SRGB;
    }

    if (!tex->create2DTextureFromRGBA8(rgba.data(), static_cast<uint32_t>(img.width), static_cast<uint32_t>(img.height), format, resources)) {
        LOG_ERROR("Failed to upload embedded GLTF texture; using fallback texture");
        return std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
    }

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
        v.position = glm::vec3(posData[3*i+0], posData[3*i+1], posData[3*i+2]);

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
        mat->albedoTexture = loadTexture(imgIdx, model, resources, TextureUsage::Albedo);
        mat->uboData.hasAlbedoMap = 1.0f;
    } else {
        mat->albedoTexture = std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
        mat->uboData.hasAlbedoMap = 0.0f;
    }

    // Normal
    if (gltfMat.normalTexture.index >= 0) {
        int imgIdx = model.textures[gltfMat.normalTexture.index].source;
        mat->normalTexture = loadTexture(imgIdx, model, resources, TextureUsage::Normal);
        mat->uboData.useNormalMap = 1.0f;
    } else {
        mat->normalTexture = std::shared_ptr<Texture>(Texture::getFlatNormalTexture(resources), [](Texture*){});
        mat->uboData.useNormalMap = 0.0f;
    }

    // MetallicRoughness
    if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
        int imgIdx = model.textures[gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index].source;
        mat->metallicRoughnessTexture = loadTexture(imgIdx, model, resources, TextureUsage::MetallicRoughness);
        mat->uboData.hasMetallicRoughnessMap = 1.0f;
    } else {
        mat->metallicRoughnessTexture = std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
        mat->uboData.hasMetallicRoughnessMap = 0.0f;
    }

    // Occlusion / AO
    if (gltfMat.occlusionTexture.index >= 0) {
        int imgIdx = model.textures[gltfMat.occlusionTexture.index].source;
        mat->aoTexture = loadTexture(imgIdx, model, resources, TextureUsage::AO);
        mat->uboData.hasAOMap = 1.0f;
    } else {
        mat->aoTexture = std::shared_ptr<Texture>(Texture::getWhiteTexture(resources), [](Texture*){});
        mat->uboData.hasAOMap = 0.0f;
    }

    // Emissive
    if (gltfMat.emissiveTexture.index >= 0) {
        int imgIdx = model.textures[gltfMat.emissiveTexture.index].source;
        mat->emissiveTexture = loadTexture(imgIdx, model, resources, TextureUsage::Emissive);
        mat->uboData.hasEmissiveMap = 1.0f;
    } else {
        mat->emissiveTexture = std::shared_ptr<Texture>(Texture::getBlackTexture(resources), [](Texture*){});
        mat->uboData.hasEmissiveMap = 0.0f;
    }

    mat->setMetallic(static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor));
    mat->setRoughness(static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor));
    if (!gltfMat.emissiveFactor.empty()) {
        float strength = 1.0f; // tinygltf or gltf might define strength or factor. We'll map first component or average.
        mat->setEmissiveStrength(strength);
    }
    mat->dirty = true;

    // Create the graphics pipeline
    bool ok = mat->create("shaders/gbuffer_vert.spv",
                         "shaders/gbuffer_frag.spv",
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
                     RenderSceneCache& scene)
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
            mesh->hasNormals = prim.attributes.find("NORMAL") != prim.attributes.end();
            mesh->hasUVs = prim.attributes.find("TEXCOORD_0") != prim.attributes.end();
            mesh->hasTangents = prim.attributes.find("TANGENT") != prim.attributes.end();
            mesh->normalsGenerated = false;
            mesh->tangentsGenerated = false;

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
                    defaultMat->normalTexture = std::shared_ptr<Texture>(Texture::getFlatNormalTexture(resources), [](Texture*){});
                    defaultMat->setMetallic(0.0f);
                    defaultMat->setRoughness(0.5f);
                    defaultMat->dirty = true;
                    defaultMat->create("shaders/gbuffer_vert.spv",
                                      "shaders/gbuffer_frag.spv",
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