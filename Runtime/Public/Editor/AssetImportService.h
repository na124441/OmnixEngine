#pragma once
#include "Runtime/Public/AssetRegistry.h"
#include <string>

namespace eng::runtime {

class AssetImportService {
public:
    // Imports a model (.obj) from sourcePath:
    // - copies to Assets/Meshes/
    // - registers in AssetRegistry
    // - saves AssetRegistry.json
    // Returns the relative path of the imported asset, or empty string on failure.
    static std::string ImportModel(const std::string& sourcePath, AssetRegistry* registry);
};

} // namespace eng::runtime
