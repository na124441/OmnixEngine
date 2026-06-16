#pragma once
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/OmnixMeshFormat.h" // reuse Vec3, BoundingBox, BoundingSphere
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

struct MeshMetadata
{
    AssetHandle handle;

    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t submeshCount = 0;

    BoundingBox bounds;
    BoundingSphere sphere;

    bool hasNormals = false;
    bool hasUVs = false;
    bool hasTangents = false;
    bool hasUV0 = false; // keep for compatibility
    bool hasSkeleton = false;
    bool normalsGenerated = false;
    bool tangentsGenerated = false;

    uint64_t sourceTimestamp = 0;
    uint64_t importTimestamp = 0;
};

inline bool SaveMeshMetadata(const MeshMetadata& meta, const std::string& filepath) {
    nlohmann::json jsonMeta = nlohmann::json::object();
    jsonMeta["handle"] = meta.handle.value;
    jsonMeta["vertexCount"] = meta.vertexCount;
    jsonMeta["indexCount"] = meta.indexCount;
    jsonMeta["submeshCount"] = meta.submeshCount;

    nlohmann::json bbox = nlohmann::json::object();
    bbox["min"] = { meta.bounds.min.x, meta.bounds.min.y, meta.bounds.min.z };
    bbox["max"] = { meta.bounds.max.x, meta.bounds.max.y, meta.bounds.max.z };
    jsonMeta["bounds"] = bbox;

    nlohmann::json bsphere = nlohmann::json::object();
    bsphere["center"] = { meta.sphere.center.x, meta.sphere.center.y, meta.sphere.center.z };
    bsphere["radius"] = meta.sphere.radius;
    jsonMeta["sphere"] = bsphere;

    jsonMeta["hasNormals"] = meta.hasNormals;
    jsonMeta["hasUVs"] = meta.hasUVs;
    jsonMeta["hasTangents"] = meta.hasTangents;
    jsonMeta["hasUV0"] = meta.hasUV0;
    jsonMeta["hasSkeleton"] = meta.hasSkeleton;
    jsonMeta["normalsGenerated"] = meta.normalsGenerated;
    jsonMeta["tangentsGenerated"] = meta.tangentsGenerated;
    jsonMeta["sourceTimestamp"] = meta.sourceTimestamp;
    jsonMeta["importTimestamp"] = meta.importTimestamp;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    file << jsonMeta.dump(4);
    return true;
}

inline bool LoadMeshMetadata(MeshMetadata& meta, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json jsonMeta;
    try {
        file >> jsonMeta;
    } catch (const std::exception&) {
        return false;
    }

    if (jsonMeta.contains("handle") && jsonMeta["handle"].is_number()) {
        meta.handle = AssetHandle{jsonMeta["handle"].get<uint64_t>()};
    }
    if (jsonMeta.contains("vertexCount") && jsonMeta["vertexCount"].is_number()) {
        meta.vertexCount = jsonMeta["vertexCount"].get<uint32_t>();
    }
    if (jsonMeta.contains("indexCount") && jsonMeta["indexCount"].is_number()) {
        meta.indexCount = jsonMeta["indexCount"].get<uint32_t>();
    }
    if (jsonMeta.contains("submeshCount") && jsonMeta["submeshCount"].is_number()) {
        meta.submeshCount = jsonMeta["submeshCount"].get<uint32_t>();
    }

    if (jsonMeta.contains("bounds") && jsonMeta["bounds"].is_object()) {
        auto bbox = jsonMeta["bounds"];
        if (bbox.contains("min") && bbox["min"].is_array() && bbox["min"].size() == 3) {
            meta.bounds.min.x = bbox["min"][0].get<float>();
            meta.bounds.min.y = bbox["min"][1].get<float>();
            meta.bounds.min.z = bbox["min"][2].get<float>();
        }
        if (bbox.contains("max") && bbox["max"].is_array() && bbox["max"].size() == 3) {
            meta.bounds.max.x = bbox["max"][0].get<float>();
            meta.bounds.max.y = bbox["max"][1].get<float>();
            meta.bounds.max.z = bbox["max"][2].get<float>();
        }
    }

    if (jsonMeta.contains("sphere") && jsonMeta["sphere"].is_object()) {
        auto bsphere = jsonMeta["sphere"];
        if (bsphere.contains("center") && bsphere["center"].is_array() && bsphere["center"].size() == 3) {
            meta.sphere.center.x = bsphere["center"][0].get<float>();
            meta.sphere.center.y = bsphere["center"][1].get<float>();
            meta.sphere.center.z = bsphere["center"][2].get<float>();
        }
        if (bsphere.contains("radius") && bsphere["radius"].is_number()) {
            meta.sphere.radius = bsphere["radius"].get<float>();
        }
    }

    if (jsonMeta.contains("hasNormals") && jsonMeta["hasNormals"].is_boolean()) {
        meta.hasNormals = jsonMeta["hasNormals"].get<bool>();
    }
    if (jsonMeta.contains("hasUVs") && jsonMeta["hasUVs"].is_boolean()) {
        meta.hasUVs = jsonMeta["hasUVs"].get<bool>();
    }
    if (jsonMeta.contains("hasTangents") && jsonMeta["hasTangents"].is_boolean()) {
        meta.hasTangents = jsonMeta["hasTangents"].get<bool>();
    }
    if (jsonMeta.contains("hasUV0") && jsonMeta["hasUV0"].is_boolean()) {
        meta.hasUV0 = jsonMeta["hasUV0"].get<bool>();
    }
    if (jsonMeta.contains("hasSkeleton") && jsonMeta["hasSkeleton"].is_boolean()) {
        meta.hasSkeleton = jsonMeta["hasSkeleton"].get<bool>();
    }
    if (jsonMeta.contains("normalsGenerated") && jsonMeta["normalsGenerated"].is_boolean()) {
        meta.normalsGenerated = jsonMeta["normalsGenerated"].get<bool>();
    }
    if (jsonMeta.contains("tangentsGenerated") && jsonMeta["tangentsGenerated"].is_boolean()) {
        meta.tangentsGenerated = jsonMeta["tangentsGenerated"].get<bool>();
    }
    if (jsonMeta.contains("sourceTimestamp") && jsonMeta["sourceTimestamp"].is_number()) {
        meta.sourceTimestamp = jsonMeta["sourceTimestamp"].get<uint64_t>();
    }
    if (jsonMeta.contains("importTimestamp") && jsonMeta["importTimestamp"].is_number()) {
        meta.importTimestamp = jsonMeta["importTimestamp"].get<uint64_t>();
    }

    return true;
}
