//============================================================================
// Quaternion.h - Simple Quaternion for Rotations
//============================================================================

#pragma once

struct Quaternion {
    float x, y, z, w;

    // Constructors
    Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quaternion(float xx, float yy, float zz, float ww) : x(xx), y(yy), z(zz), w(ww) {}

    // Assignment
    Quaternion& operator=(const Quaternion& other) {
        x = other.x;
        y = other.y;
        z = other.z;
        w = other.w;
        return *this;
    }
};

//============================================================================
// END OF FILE
//================================================================//
// Created by nayan on 11/25/2025.
//

#ifndef OMNIXENGINE_QUATERION_H
#define OMNIXENGINE_QUATERION_H

#endif //OMNIXENGINE_QUATERION_H