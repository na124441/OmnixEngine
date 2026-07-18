#pragma once
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetType.h"
#include "Runtime/ReloadState.h"
#include <string>

namespace eng::runtime {

    struct ReloadEvent
    {
        AssetHandle handle;
        AssetType type = AssetType::Unknown;
        std::string sourcePath;

        ReloadState state = ReloadState::NotQueued;

        double reloadTimeMs = 0.0;
        uint32_t dependentCount = 0;

        std::string message;
    };

} // namespace eng::runtime
