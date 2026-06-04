#pragma once

#include "../../Scene/Vector3.h"
#include "../../Scene/Quaternion.h"

struct Transform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
};