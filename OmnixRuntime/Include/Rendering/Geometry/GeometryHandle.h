#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace eng::renderer {

struct GeometryHandle {
    uint32_t index = 0xFFFFFFFF;
    uint32_t generation = 0;

    constexpr GeometryHandle() noexcept = default;
    constexpr GeometryHandle(uint32_t idx, uint32_t gen) noexcept : index(idx), generation(gen) {}

    constexpr bool IsValid() const noexcept {
        return index != 0xFFFFFFFF;
    }

    constexpr bool operator==(const GeometryHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    constexpr bool operator!=(const GeometryHandle& other) const noexcept {
        return !(*this == other);
    }

    constexpr bool operator<(const GeometryHandle& other) const noexcept {
        if (index != other.index) {
            return index < other.index;
        }
        return generation < other.generation;
    }

    std::string ToString() const {
        if (!IsValid()) {
            return "GeometryHandle(Invalid)";
        }
        return "GeometryHandle(" + std::to_string(index) + ", gen=" + std::to_string(generation) + ")";
    }
};

struct VirtualGeometryHandle {
    uint32_t index = 0xFFFFFFFF;
    uint32_t generation = 0;

    constexpr VirtualGeometryHandle() noexcept = default;
    constexpr VirtualGeometryHandle(uint32_t idx, uint32_t gen) noexcept : index(idx), generation(gen) {}

    constexpr bool IsValid() const noexcept {
        return index != 0xFFFFFFFF;
    }

    constexpr bool operator==(const VirtualGeometryHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    constexpr bool operator!=(const VirtualGeometryHandle& other) const noexcept {
        return !(*this == other);
    }

    constexpr bool operator<(const VirtualGeometryHandle& other) const noexcept {
        if (index != other.index) {
            return index < other.index;
        }
        return generation < other.generation;
    }

    std::string ToString() const {
        if (!IsValid()) {
            return "VirtualGeometryHandle(Invalid)";
        }
        return "VirtualGeometryHandle(" + std::to_string(index) + ", gen=" + std::to_string(generation) + ")";
    }
};

} // namespace eng::renderer

namespace std {
    template <>
    struct hash<eng::renderer::GeometryHandle> {
        std::size_t operator()(const eng::renderer::GeometryHandle& handle) const noexcept {
            return std::hash<uint64_t>{}((static_cast<uint64_t>(handle.index) << 32) | handle.generation);
        }
    };

    template <>
    struct hash<eng::renderer::VirtualGeometryHandle> {
        std::size_t operator()(const eng::renderer::VirtualGeometryHandle& handle) const noexcept {
            return std::hash<uint64_t>{}((static_cast<uint64_t>(handle.index) << 32) | handle.generation);
        }
    };
}
