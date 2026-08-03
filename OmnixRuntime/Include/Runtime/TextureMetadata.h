#pragma once
#include "Runtime/AssetHandle.h"
#include "Runtime/TextureFormat.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

struct TextureMetadata
{
    AssetHandle handle;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 4;

    TextureSourceFormat sourceFormat = TextureSourceFormat::Unknown;
    TextureRuntimeFormat runtimeFormat = TextureRuntimeFormat::RGBA8;

    uint32_t mipCount = 1;

    bool hasAlpha = false;
    bool isSRGB = false;
    bool generateMips = false;
    bool isCompressed = false;

    uint64_t sourceFileTimestamp = 0;
    uint64_t importTimestamp = 0;

    std::string cachePath;
};

inline bool SaveTextureMetadata(const TextureMetadata& meta, const std::string& filepath) {
    nlohmann::json jsonMeta = nlohmann::json::object();
    jsonMeta["handle"] = meta.handle.value;
    jsonMeta["width"] = meta.width;
    jsonMeta["height"] = meta.height;
    jsonMeta["channels"] = meta.channels;
    jsonMeta["sourceFormat"] = TextureSourceFormatToString(meta.sourceFormat);
    jsonMeta["runtimeFormat"] = TextureRuntimeFormatToString(meta.runtimeFormat);
    jsonMeta["mipCount"] = meta.mipCount;
    jsonMeta["hasAlpha"] = meta.hasAlpha;
    jsonMeta["isSRGB"] = meta.isSRGB;
    jsonMeta["generateMips"] = meta.generateMips;
    jsonMeta["isCompressed"] = meta.isCompressed;
    jsonMeta["sourceFileTimestamp"] = meta.sourceFileTimestamp;
    jsonMeta["importTimestamp"] = meta.importTimestamp;
    jsonMeta["cachePath"] = meta.cachePath;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    file << jsonMeta.dump(4);
    return true;
}

inline bool LoadTextureMetadata(TextureMetadata& meta, const std::string& filepath) {
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
    if (jsonMeta.contains("width") && jsonMeta["width"].is_number()) {
        meta.width = jsonMeta["width"].get<uint32_t>();
    }
    if (jsonMeta.contains("height") && jsonMeta["height"].is_number()) {
        meta.height = jsonMeta["height"].get<uint32_t>();
    }
    if (jsonMeta.contains("channels") && jsonMeta["channels"].is_number()) {
        meta.channels = jsonMeta["channels"].get<uint32_t>();
    }
    if (jsonMeta.contains("sourceFormat") && jsonMeta["sourceFormat"].is_string()) {
        meta.sourceFormat = StringToTextureSourceFormat(jsonMeta["sourceFormat"].get<std::string>());
    }
    if (jsonMeta.contains("runtimeFormat") && jsonMeta["runtimeFormat"].is_string()) {
        meta.runtimeFormat = StringToTextureRuntimeFormat(jsonMeta["runtimeFormat"].get<std::string>());
    }
    if (jsonMeta.contains("mipCount") && jsonMeta["mipCount"].is_number()) {
        meta.mipCount = jsonMeta["mipCount"].get<uint32_t>();
    }
    if (jsonMeta.contains("hasAlpha") && jsonMeta["hasAlpha"].is_boolean()) {
        meta.hasAlpha = jsonMeta["hasAlpha"].get<bool>();
    }
    if (jsonMeta.contains("isSRGB") && jsonMeta["isSRGB"].is_boolean()) {
        meta.isSRGB = jsonMeta["isSRGB"].get<bool>();
    }
    if (jsonMeta.contains("generateMips") && jsonMeta["generateMips"].is_boolean()) {
        meta.generateMips = jsonMeta["generateMips"].get<bool>();
    }
    if (jsonMeta.contains("isCompressed") && jsonMeta["isCompressed"].is_boolean()) {
        meta.isCompressed = jsonMeta["isCompressed"].get<bool>();
    }
    if (jsonMeta.contains("sourceFileTimestamp") && jsonMeta["sourceFileTimestamp"].is_number()) {
        meta.sourceFileTimestamp = jsonMeta["sourceFileTimestamp"].get<uint64_t>();
    }
    if (jsonMeta.contains("importTimestamp") && jsonMeta["importTimestamp"].is_number()) {
        meta.importTimestamp = jsonMeta["importTimestamp"].get<uint64_t>();
    }
    if (jsonMeta.contains("cachePath") && jsonMeta["cachePath"].is_string()) {
        meta.cachePath = jsonMeta["cachePath"].get<std::string>();
    }

    return true;
}
