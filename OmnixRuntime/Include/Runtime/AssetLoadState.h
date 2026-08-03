#pragma once

namespace eng::runtime {

    enum class AssetLoadState
    {
        Unloaded,
        Loading,
        Loaded,
        Failed
    };

    inline const char* AssetLoadStateToString(AssetLoadState state) noexcept {
        switch (state) {
            case AssetLoadState::Unloaded: return "Unloaded";
            case AssetLoadState::Loading: return "Loading";
            case AssetLoadState::Loaded: return "Loaded";
            case AssetLoadState::Failed: return "Failed";
            default: return "Unknown";
        }
    }

} // namespace eng::runtime
