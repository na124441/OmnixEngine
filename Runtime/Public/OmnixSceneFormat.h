#pragma once
#include "Runtime/Public/FileHeader.h"
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/BinaryReader.h"
#include "Runtime/Public/BinaryWriter.h"
#include <vector>
#include <string>

#pragma pack(push, 1)

struct SceneEntityRecord
{
    uint32_t entityID = 0;
    uint32_t generation = 0;

    uint32_t componentMask = 0;
    uint32_t parentEntityID = 0;

    bool operator==(const SceneEntityRecord& o) const noexcept {
        return entityID == o.entityID && generation == o.generation &&
               componentMask == o.componentMask && parentEntityID == o.parentEntityID;
    }
};

struct SceneHierarchyNode
{
    uint32_t entityID = 0;
    uint32_t parentEntityID = 0;

    bool operator==(const SceneHierarchyNode& o) const noexcept {
        return entityID == o.entityID && parentEntityID == o.parentEntityID;
    }
};

struct OmnixSceneHeader
{
    FileHeader file;

    uint32_t entityCount = 0;
    uint32_t componentTypeCount = 0;
    uint32_t assetReferenceCount = 0;
    uint32_t hierarchyNodeCount = 0;
    uint32_t componentBlockCount = 0;
};

#pragma pack(pop)

struct SceneComponentTypeEntry
{
    uint32_t typeID = 0;
    std::string typeName;

    bool operator==(const SceneComponentTypeEntry& o) const noexcept {
        return typeID == o.typeID && typeName == o.typeName;
    }
};

struct SceneAssetReferenceEntry
{
    AssetHandle handle;
    std::string assetPath;

    bool operator==(const SceneAssetReferenceEntry& o) const noexcept {
        return handle == o.handle && assetPath == o.assetPath;
    }
};

struct SceneComponentDataBlock
{
    uint32_t entityID = 0;
    uint32_t componentTypeID = 0;
    std::vector<uint8_t> data;

    bool operator==(const SceneComponentDataBlock& o) const noexcept {
        return entityID == o.entityID && componentTypeID == o.componentTypeID && data == o.data;
    }
};

constexpr char MAGIC_SCENE[8] = {'O', 'M', 'X', 'S', 'C', 'E', 'N', 'E'};
constexpr uint32_t OMNIX_SCENE_VERSION_MAJOR = 1;
constexpr uint32_t OMNIX_SCENE_VERSION_MINOR = 0;

struct OmnixScene
{
    OmnixSceneHeader header;
    std::string sceneName;
    std::vector<SceneComponentTypeEntry> componentTypes;
    std::vector<SceneAssetReferenceEntry> assetReferences;
    std::vector<SceneEntityRecord> entities;
    std::vector<SceneHierarchyNode> hierarchy;
    std::vector<SceneComponentDataBlock> components;
};

inline bool SerializeScene(const OmnixScene& scene, const std::string& filepath) {
    eng::runtime::BinaryWriter writer;
    writer.BeginFile(MAGIC_SCENE, OMNIX_SCENE_VERSION_MAJOR, OMNIX_SCENE_VERSION_MINOR);

    writer.WriteU32(scene.header.entityCount);
    writer.WriteU32(scene.header.componentTypeCount);
    writer.WriteU32(scene.header.assetReferenceCount);
    writer.WriteU32(scene.header.hierarchyNodeCount);
    writer.WriteU32(scene.header.componentBlockCount);

    writer.WriteString(scene.sceneName);

    // Component Type Table
    for (const auto& entry : scene.componentTypes) {
        writer.WriteU32(entry.typeID);
        writer.WriteString(entry.typeName);
    }

    // Asset Reference Table
    for (const auto& entry : scene.assetReferences) {
        writer.WriteU64(entry.handle.value);
        writer.WriteString(entry.assetPath);
    }

    // Entities
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(scene.entities.data()), scene.entities.size() * sizeof(SceneEntityRecord));

    // Hierarchy Nodes
    writer.WriteBytes(reinterpret_cast<const uint8_t*>(scene.hierarchy.data()), scene.hierarchy.size() * sizeof(SceneHierarchyNode));

    // Component Data Blocks
    for (const auto& block : scene.components) {
        writer.WriteU32(block.entityID);
        writer.WriteU32(block.componentTypeID);
        writer.WriteU32(static_cast<uint32_t>(block.data.size()));
        if (!block.data.empty()) {
            writer.WriteBytes(block.data.data(), block.data.size());
        }
    }

    return writer.SaveToFile(filepath);
}

inline bool DeserializeScene(OmnixScene& scene, const std::string& filepath) {
    eng::runtime::BinaryReader reader;
    if (!reader.LoadFromFile(filepath)) {
        return false;
    }

    if (!reader.ValidateHeaderAndChecksum(MAGIC_SCENE, OMNIX_SCENE_VERSION_MAJOR, OMNIX_SCENE_VERSION_MINOR)) {
        return false;
    }

    try {
        scene.header.entityCount = reader.ReadU32();
        scene.header.componentTypeCount = reader.ReadU32();
        scene.header.assetReferenceCount = reader.ReadU32();
        scene.header.hierarchyNodeCount = reader.ReadU32();
        scene.header.componentBlockCount = reader.ReadU32();

        scene.sceneName = reader.ReadString();

        // Component Type Table
        scene.componentTypes.resize(scene.header.componentTypeCount);
        for (uint32_t i = 0; i < scene.header.componentTypeCount; ++i) {
            scene.componentTypes[i].typeID = reader.ReadU32();
            scene.componentTypes[i].typeName = reader.ReadString();
        }

        // Asset Reference Table
        scene.assetReferences.resize(scene.header.assetReferenceCount);
        for (uint32_t i = 0; i < scene.header.assetReferenceCount; ++i) {
            scene.assetReferences[i].handle = AssetHandle{reader.ReadU64()};
            scene.assetReferences[i].assetPath = reader.ReadString();
        }

        // Entities
        scene.entities.resize(scene.header.entityCount);
        if (scene.header.entityCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(scene.entities.data()), scene.header.entityCount * sizeof(SceneEntityRecord));
        }

        // Hierarchy
        scene.hierarchy.resize(scene.header.hierarchyNodeCount);
        if (scene.header.hierarchyNodeCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(scene.hierarchy.data()), scene.header.hierarchyNodeCount * sizeof(SceneHierarchyNode));
        }

        // Component Data Blocks
        scene.components.resize(scene.header.componentBlockCount);
        for (uint32_t i = 0; i < scene.header.componentBlockCount; ++i) {
            scene.components[i].entityID = reader.ReadU32();
            scene.components[i].componentTypeID = reader.ReadU32();
            uint32_t size = reader.ReadU32();
            scene.components[i].data.resize(size);
            if (size > 0) {
                reader.ReadBytes(scene.components[i].data.data(), size);
            }
        }

    } catch (const std::exception&) {
        return false;
    }

    return true;
}
