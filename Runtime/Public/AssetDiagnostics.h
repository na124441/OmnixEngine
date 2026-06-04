#pragma once
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/AssetType.h"
#include <string>

namespace eng::runtime {

    struct AssetDiagnosticInfo {
        AssetHandle handle;
        AssetType type = AssetType::Unknown;
        std::string sourcePath;
        std::string importedPath;
        std::string lastError;
        uint32_t cacheHits = 0;
        uint32_t cacheMisses = 0;
        uint32_t loadTimeMs = 0;
    };

    struct AssetManagerStats {
        uint32_t totalLoadsAttempted = 0;
        uint32_t totalLoadSuccesses = 0;
        uint32_t totalLoadFailures = 0;
        uint32_t totalCacheHits = 0;
        uint32_t totalCacheMisses = 0;
        uint32_t totalDependenciesLoaded = 0;
    };

} // namespace eng::runtime
