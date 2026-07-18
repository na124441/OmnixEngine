//============================================================================
// SceneSerializer.h - Refactored to use rapidjson
//============================================================================

#pragma once

#include <string>
#include <memory>
#include <vector>
#include "../ThirdParty/rapidjson-master/include/rapidjson/document.h"

// Forward declarations
class Scene;
class SceneObject;
class Prefab;

class SceneSerializer {
public:
    static bool SaveScene(Scene* scene, const std::string& filePath);
    static bool SavePrefab(std::shared_ptr<Prefab> prefab, const std::string& filePath);
    static bool ValidateOutputPath(const std::string& filePath);
    static std::vector<std::string> GetSupportedExtensions();

private:
    static void SerializeSceneObject(rapidjson::Document& doc, std::shared_ptr<SceneObject> object, bool includeChildren = false);
    static void SerializeTransform(rapidjson::Value& transformValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> object);
    static void SerializeComponents(rapidjson::Value& componentsValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> object);
    static void SerializeHierarchy(rapidjson::Value& hierarchyValue, rapidjson::Document::AllocatorType& allocator, Scene* scene);
    static void SerializeSceneMetadata(rapidjson::Value& metadataValue, rapidjson::Document::AllocatorType& allocator, Scene* scene);
    static bool IsPrefabInstance(std::shared_ptr<SceneObject> object);
    static std::string GetPrefabPath(std::shared_ptr<SceneObject> object);
    static void SerializePrefabOverrides(rapidjson::Value& overridesValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> object);
    static bool WriteJSONToFile(const rapidjson::Document& doc, const std::string& filePath, bool prettyPrint = true);
    static void SerializePrefabMetadata(rapidjson::Value& metadataValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<Prefab> prefab);
    static void SerializeChildren(rapidjson::Value& childrenValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> parent);
};
