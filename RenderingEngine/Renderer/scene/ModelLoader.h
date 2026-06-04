#pragma once
#include <string>
#include <vector>
#include "Mesh.h"

namespace eng::renderer {

struct EngineResources;

/**
 * @brief Utility class to load 3D models from disk.
 * Currently supports OBJ format.
 */
class ModelLoader {
public:
    /**
     * @brief Loads an OBJ file and populates the given Mesh.
     * @param path Path to the .obj file.
     * @param outMesh Mesh object to populate.
     * @param resources Engine resources for buffer creation.
     * @return true if successful.
     */
    static bool LoadOBJ(const std::string& path, Mesh& outMesh, EngineResources& resources);
};

} // namespace eng::renderer
