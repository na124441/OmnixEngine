#pragma once
#include "Runtime/OmnixMeshFormat.h"
#include <cmath>

inline bool IsFiniteFloat(float f) noexcept {
    return std::isfinite(f);
}

inline bool IsFiniteVec3(const Vec3& v) noexcept {
    return IsFiniteFloat(v.x) && IsFiniteFloat(v.y) && IsFiniteFloat(v.z);
}

inline bool IsFiniteVec4(const Vec4& v) noexcept {
    return IsFiniteFloat(v.x) && IsFiniteFloat(v.y) && IsFiniteFloat(v.z) && IsFiniteFloat(v.w);
}

inline bool IsFiniteVec2(const Vec2& v) noexcept {
    return IsFiniteFloat(v.x) && IsFiniteFloat(v.y);
}

inline bool ValidateMeshIntegrity(const OmnixMesh& mesh) noexcept {
    if (mesh.header.vertexCount == 0 || mesh.header.indexCount == 0) {
        return false;
    }

    if (mesh.header.indexCount % 3 != 0) {
        return false;
    }

    // Check indices
    for (uint32_t idx : mesh.indices) {
        if (idx >= mesh.header.vertexCount) {
            return false; // Out of bounds index
        }
    }

    // Check vertices
    if (mesh.header.hasSkeleton) {
        for (const auto& v : mesh.skinnedVertices) {
            if (!IsFiniteVec3(v.position) || !IsFiniteVec3(v.normal) || !IsFiniteVec4(v.tangent) || !IsFiniteVec2(v.uv0)) {
                return false;
            }
        }
    } else {
        for (const auto& v : mesh.vertices) {
            if (!IsFiniteVec3(v.position) || !IsFiniteVec3(v.normal) || !IsFiniteVec4(v.tangent) || !IsFiniteVec2(v.uv0)) {
                return false;
            }
        }
    }

    // Check bounds
    if (!IsFiniteVec3(mesh.header.bounds.min) || !IsFiniteVec3(mesh.header.bounds.max)) {
        return false;
    }

    if (mesh.header.bounds.min.x > mesh.header.bounds.max.x ||
        mesh.header.bounds.min.y > mesh.header.bounds.max.y ||
        mesh.header.bounds.min.z > mesh.header.bounds.max.z) {
        return false; // Invalid AABB bounds
    }

    if (!IsFiniteVec3(mesh.header.sphere.center) || !IsFiniteFloat(mesh.header.sphere.radius) || mesh.header.sphere.radius < 0.0f) {
        return false; // Invalid sphere bounds
    }

    return true;
}
