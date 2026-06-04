//============================================================================
// Vector3.h - Simple 3D Vector
//============================================================================

#pragma once

struct Vector3 {
    float x, y, z;

    // Constructors
    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
    Vector3(const Vector3& other) = default; // Explicitly default the copy constructor

    // Basic operations
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    Vector3& operator=(const Vector3& other) = default; // Explicitly default the copy assignment
};

//============================================================================
// END OF FILE
//================================================================//
// Created by nayan on 11/25/2025.
//
