#pragma once
#include "Scene/SceneValidationReport.h"
#include <string>

#include "../ThirdParty/rapidjson-master/include/rapidjson/document.h"

// Forward declarations
namespace eng::runtime {
    class AssetRegistry;
}
class PrefabRegistry;

class SceneValidator {
public:
    SceneValidator() = default;
    ~SceneValidator() = default;

    /**
     * @brief Validates a scene file path.
     * Parses the file to JSON and runs ValidateSceneDocument.
     */
    SceneValidationReport ValidateSceneFile(
        const std::string& path,
        const eng::runtime::AssetRegistry* assetRegistry,
        const PrefabRegistry* prefabRegistry = nullptr
    );

    /**
     * @brief Validates a parsed rapidjson scene document.
     */
    SceneValidationReport ValidateSceneDocument(
        const rapidjson::Document& doc,
        const eng::runtime::AssetRegistry* assetRegistry,
        const PrefabRegistry* prefabRegistry = nullptr
    );
};
