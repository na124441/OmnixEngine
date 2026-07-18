//============================================================================
// Quaternion.h - Simple Quaternion for Rotations
//============================================================================

#pragma once

struct Quaternion {
    float x, y, z, w;

    // Constructors
    Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quaternion(float xx, float yy, float zz, float ww) : x(xx), y(yy), z(zz), w(ww) {}
    Quaternion(const Quaternion& other) = default; // Explicitly default the copy constructor

    // Assignment operator
    Quaternion& operator=(const Quaternion& other) = default; // Explicitly default the copy assignment
};

//
// Created by nayan on 11/25/2025.
//

#ifndef OMNIXENGINE_QUATERNION_H
#define OMNIXENGINE_QUATERNION_H

#endif //OMNIXENGINE_QUATERNION_H