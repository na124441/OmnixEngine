#include "Runtime/RuntimeLoaders.h"
#include "Runtime/TextureImporter.h"
#include "Runtime/OmnixMeshFormat.h"
#include "Runtime/OmnixMaterialFormat.h"
#include "Runtime/PackageManager.h"
#include "Core/Logging/Logger.h"
#include <filesystem>

namespace eng::runtime {

    // =========================================================================
    // Texture Loader
    // =========================================================================

    bool TextureLoader::Load(const AssetMetadata& metadata, RuntimeAsset** outAsset)
    {
        if (!outAsset) return false;

        std::vector<uint8_t> pixelData;
        OmnixTextureHeader header;
        bool loaded = false;

        if (GetPackageManager().Contains(metadata.handle)) {
            std::vector<uint8_t> payload = GetPackageManager().ReadAssetPayload(metadata.handle);
            if (!payload.empty()) {
                TextureImporter importer;
                if (importer.LoadOmnixTextureFromMemory(payload.data(), payload.size(), pixelData, header)) {
                    loaded = true;
                }
            }
        }

        if (!loaded) {
            if (!std::filesystem::exists(metadata.importedPath)) {
                LOG_ERROR("[TextureLoader] File not found: %s", metadata.importedPath.c_str());
                return false;
            }

            TextureImporter importer;
            if (!importer.LoadOmnixTexture(metadata.importedPath, pixelData, header)) {
                LOG_ERROR("[TextureLoader] Failed to parse texture file: %s", metadata.importedPath.c_str());
                return false;
            }
        }

        RuntimeTexture* texture = new RuntimeTexture();
        texture->width = header.width;
        texture->height = header.height;
        texture->channels = header.channels;
        texture->mipCount = header.mipCount;
        texture->runtimeFormat = header.runtimeFormat;
        texture->isSRGB = (header.isSRGB != 0);
        texture->isCompressed = (header.isCompressed != 0);
        texture->pixelData = std::move(pixelData);

        *outAsset = texture;
        return true;
    }

    void TextureLoader::Unload(RuntimeAsset* asset)
    {
        delete asset;
    }

    // =========================================================================
    // Mesh Loader
    // =========================================================================

    bool MeshLoader::Load(const AssetMetadata& metadata, RuntimeAsset** outAsset)
    {
        if (!outAsset) return false;

        OmnixMesh meshData;
        bool loaded = false;

        if (GetPackageManager().Contains(metadata.handle)) {
            std::vector<uint8_t> payload = GetPackageManager().ReadAssetPayload(metadata.handle);
            if (!payload.empty()) {
                if (DeserializeMeshFromMemory(meshData, payload.data(), payload.size())) {
                    loaded = true;
                }
            }
        }

        if (!loaded) {
            if (!std::filesystem::exists(metadata.importedPath)) {
                LOG_ERROR("[MeshLoader] File not found: %s", metadata.importedPath.c_str());
                return false;
            }

            if (!DeserializeMesh(meshData, metadata.importedPath)) {
                LOG_ERROR("[MeshLoader] Failed to deserialize mesh file: %s", metadata.importedPath.c_str());
                return false;
            }
        }

        RuntimeMesh* mesh = new RuntimeMesh();
        mesh->vertexCount = meshData.header.vertexCount;
        mesh->indexCount = meshData.header.indexCount;
        mesh->vertexStride = meshData.header.vertexStride;
        mesh->submeshCount = meshData.header.submeshCount;
        mesh->hasSkeleton = (meshData.header.hasSkeleton != 0);
        mesh->bounds = meshData.header.bounds;
        mesh->sphere = meshData.header.sphere;

        mesh->vertices = std::move(meshData.vertices);
        mesh->skinnedVertices = std::move(meshData.skinnedVertices);
        mesh->indices = std::move(meshData.indices);
        mesh->submeshes = std::move(meshData.submeshes);
        mesh->materialSlots = std::move(meshData.materialSlots);
        mesh->skeletonAssetPath = std::move(meshData.skeletonAssetPath);

        *outAsset = mesh;
        return true;
    }

    void MeshLoader::Unload(RuntimeAsset* asset)
    {
        delete asset;
    }

    // =========================================================================
    // Material Loader
    // =========================================================================

    bool MaterialLoader::Load(const AssetMetadata& metadata, RuntimeAsset** outAsset)
    {
        if (!outAsset) return false;

        OmnixMaterial materialData;
        bool loaded = false;

        if (GetPackageManager().Contains(metadata.handle)) {
            std::vector<uint8_t> payload = GetPackageManager().ReadAssetPayload(metadata.handle);
            if (!payload.empty()) {
                if (DeserializeMaterialFromMemory(materialData, payload.data(), payload.size())) {
                    loaded = true;
                }
            }
        }

        if (!loaded) {
            if (!std::filesystem::exists(metadata.importedPath)) {
                LOG_ERROR("[MaterialLoader] File not found: %s", metadata.importedPath.c_str());
                return false;
            }

            if (!DeserializeMaterial(materialData, metadata.importedPath)) {
                LOG_ERROR("[MaterialLoader] Failed to deserialize material file: %s", metadata.importedPath.c_str());
                return false;
            }
        }

        RuntimeMaterial* material = new RuntimeMaterial();
        material->shader = materialData.header.shader;
        material->materialName = materialData.name;
        material->textures = std::move(materialData.textures);
        material->scalars = std::move(materialData.scalars);
        material->vectors = std::move(materialData.vectors);

        *outAsset = material;
        return true;
    }

    void MaterialLoader::Unload(RuntimeAsset* asset)
    {
        delete asset;
    }

} // namespace eng::runtime
