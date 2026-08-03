//============================================================================
// SceneLoader.h - Refactored to use rapidjson
//============================================================================

#pragma once

#include <string>
#include <memory>
#include <vector>
#include "../ThirdParty/rapidjson-master/include/rapidjson/document.h"

// Forward declarations
class Scene;
class SceneObject;

/**
 * @brief SceneLoader - Scene construction from JSON files
 */
class SceneLoader {
public:
    // Main loading methods
    static Scene* LoadFromFile(const std::string& filePath);

    static std::shared_ptr<SceneObject> LoadPrefabInsideScene(
        const rapidjson::Value& fileData,
        Scene* scene
    );

    // Utility methods
    static bool ValidateSceneFile(const std::string& filePath);
    static std::vector<std::string> GetSupportedExtensions();

private:
    // Internal helpers
    static std::string ReadFileToString(const std::string& filePath);

    static std::shared_ptr<SceneObject> CreateObjectFromData(
        const rapidjson::Value& objectData,
        Scene* scene
    );

    static void BuildHierarchy(
        std::vector<std::shared_ptr<SceneObject>>& objects,
        const rapidjson::Value& hierarchyData
    );

    static void ApplySceneMetadata(
        Scene* scene,
        const rapidjson::Value& metadata
    );

    static void ApplyComponentOverrides(
        std::shared_ptr<SceneObject> object,
        const rapidjson::Value& overrides
    );

    static void BuildPrefabChildren(
        std::shared_ptr<SceneObject> parent,
        const rapidjson::Value& childrenData,
        Scene* scene
    );
};


//============================================================================
// END OF FILE
//===============================================================