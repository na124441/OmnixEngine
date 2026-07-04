#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <mutex>
#include <vector>
#include <string_view>

namespace eng::renderer {

enum class GeometryPageState : uint8_t {
    Unloaded,
    Requested,
    Reading,
    Decompressing,
    UploadQueued,
    Resident,
    EvictionPending,
    Failed
};

inline std::string_view GetPageStateName(GeometryPageState state) {
    switch (state) {
        case GeometryPageState::Unloaded:        return "Unloaded";
        case GeometryPageState::Requested:       return "Requested";
        case GeometryPageState::Reading:         return "Reading";
        case GeometryPageState::Decompressing:   return "Decompressing";
        case GeometryPageState::UploadQueued:    return "UploadQueued";
        case GeometryPageState::Resident:        return "Resident";
        case GeometryPageState::EvictionPending: return "EvictionPending";
        case GeometryPageState::Failed:          return "Failed";
    }
    return "Unknown";
}

struct PageResidencyState {
    mutable std::mutex mutex;
    GeometryPageState state = GeometryPageState::Unloaded;
    std::chrono::steady_clock::time_point lastStateTransitionTime = std::chrono::steady_clock::now();
    uint32_t retryCount = 0;
    std::string failureReason = "";

    PageResidencyState() = default;
    
    PageResidencyState(const PageResidencyState&) = delete;
    PageResidencyState& operator=(const PageResidencyState&) = delete;

    PageResidencyState(PageResidencyState&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mutex);
        state = other.state;
        lastStateTransitionTime = other.lastStateTransitionTime;
        retryCount = other.retryCount;
        failureReason = std::move(other.failureReason);
    }

    PageResidencyState& operator=(PageResidencyState&& other) noexcept {
        if (this != &other) {
            std::lock(mutex, other.mutex);
            std::lock_guard<std::mutex> lockThis(mutex, std::adopt_lock);
            std::lock_guard<std::mutex> lockOther(other.mutex, std::adopt_lock);
            state = other.state;
            lastStateTransitionTime = other.lastStateTransitionTime;
            retryCount = other.retryCount;
            failureReason = std::move(other.failureReason);
        }
        return *this;
    }

    bool TransitionTo(GeometryPageState newState, std::string_view reason = "") {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Legal state transition checks
        bool legal = false;
        switch (state) {
            case GeometryPageState::Unloaded:
                legal = (newState == GeometryPageState::Requested);
                break;
            case GeometryPageState::Requested:
                legal = (newState == GeometryPageState::Reading || newState == GeometryPageState::Failed || newState == GeometryPageState::Unloaded);
                break;
            case GeometryPageState::Reading:
                legal = (newState == GeometryPageState::Decompressing || newState == GeometryPageState::Failed || newState == GeometryPageState::Unloaded);
                break;
            case GeometryPageState::Decompressing:
                legal = (newState == GeometryPageState::UploadQueued || newState == GeometryPageState::Failed || newState == GeometryPageState::Unloaded);
                break;
            case GeometryPageState::UploadQueued:
                legal = (newState == GeometryPageState::Resident || newState == GeometryPageState::Failed || newState == GeometryPageState::Unloaded);
                break;
            case GeometryPageState::Resident:
                legal = (newState == GeometryPageState::EvictionPending || newState == GeometryPageState::Unloaded);
                break;
            case GeometryPageState::EvictionPending:
                legal = (newState == GeometryPageState::Unloaded || newState == GeometryPageState::Resident);
                break;
            case GeometryPageState::Failed:
                legal = (newState == GeometryPageState::Requested || newState == GeometryPageState::Unloaded);
                break;
        }

        if (legal) {
            state = newState;
            lastStateTransitionTime = std::chrono::steady_clock::now();
            if (newState == GeometryPageState::Failed) {
                failureReason = reason;
                retryCount++;
            } else if (newState == GeometryPageState::Requested) {
                // Keep retries if transitioning back, or reset
            } else if (newState == GeometryPageState::Resident) {
                failureReason = "";
                retryCount = 0;
            }
            return true;
        }
        return false;
    }
};

// GPU Layout matching 12.3 Virtual Page Table
struct GPUPageMapping {
    uint32_t physicalPage = 0xFFFFFFFF; // Sentinel for non-resident
    uint32_t generation = 0;
    uint32_t flags = 0; // Bit 0: IsResident, Bit 1: IsRoot
    uint32_t reserved = 0;
};

// GPU request layout matching 12.5 GPU Request Generation
struct GeometryStreamingRequest {
    uint32_t assetID;
    uint32_t pageID;
    float projectedImportance;
    uint16_t viewMask;
    uint16_t flags;
};

} // namespace eng::renderer
