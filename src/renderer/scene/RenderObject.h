#pragma once
#include <glm/glm.hpp>
#include "Mesh.h"
#include "Material.h"

/// One renderable entity – a pointer to a mesh, a pointer to a material,
/// and a model matrix.  All pointers are non‑owning; the scene owns the
/// objects themselves (see Scene.h).
struct RenderObject {
    Mesh*       mesh     = nullptr;   // non‑owning – stored elsewhere
    Material*   material = nullptr;   // non‑owning – stored elsewhere
    glm::mat4   transform = glm::mat4(1.0f);
};
