#pragma once
#include <glm/glm.hpp>

/* Matches the layout in the fragment shader (set = 2, binding = 0) */
struct LightData
{
    alignas(16) glm::vec4 direction; // xyz = world‑space light direction, w = unused
    alignas(16) glm::vec4 color;     // rgb = illumination colour, w = intensity
    alignas(16) glm::vec4 ambient;   // rgb = ambient colour, w = intensity
};
