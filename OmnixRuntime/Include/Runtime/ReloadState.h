#pragma once

namespace eng::runtime {

    enum class ReloadState
    {
        NotQueued,
        Queued,
        Reloading,
        Failed,
        Complete
    };

    inline const char* ReloadStateToString(ReloadState state) noexcept {
        switch (state) {
            case ReloadState::NotQueued: return "NotQueued";
            case ReloadState::Queued: return "Queued";
            case ReloadState::Reloading: return "Reloading";
            case ReloadState::Failed: return "Failed";
            case ReloadState::Complete: return "Complete";
            default: return "Unknown";
        }
    }

} // namespace eng::runtime
