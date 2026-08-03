#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <algorithm>

struct AssetHandle {
    uint64_t value = 0;

    constexpr AssetHandle() noexcept : value(0) {}
    constexpr explicit AssetHandle(uint64_t val) noexcept : value(val) {}

    constexpr bool IsValid() const noexcept {
        return value != 0;
    }

    constexpr bool operator==(const AssetHandle& other) const noexcept {
        return value == other.value;
    }

    constexpr bool operator!=(const AssetHandle& other) const noexcept {
        return value != other.value;
    }

    constexpr bool operator<(const AssetHandle& other) const noexcept {
        return value < other.value;
    }
};

constexpr AssetHandle InvalidAssetHandle{0};

inline uint64_t HashFNV1a(const std::string& str) noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Specialize std::hash for AssetHandle so it can be used in std::unordered_map
namespace std {
    template <>
    struct hash<AssetHandle> {
        std::size_t operator()(const AssetHandle& handle) const noexcept {
            return std::hash<uint64_t>{}(handle.value);
        }
    };
}
