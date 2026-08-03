#pragma once
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetType.h"
#include <string>
#include <vector>

struct AssetMetadata
{
    AssetHandle handle;
    AssetType type = AssetType::Unknown;

    std::string sourcePath;
    std::string importedPath;

    uint64_t importTimestamp = 0;

    bool isImported = false;
    bool isDirty = true;
    bool sourceMissing = false;

    std::vector<AssetHandle> dependencies;
};
