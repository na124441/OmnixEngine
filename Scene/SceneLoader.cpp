//============================================================================
// SceneLoader.cpp - Refactored to use rapidjson
//============================================================================

#include "SceneLoader.h"
#include "Scene.h"
#include "SceneObject.h"
#include "PrefabRegistry.h"
#include "ComponentFactory.h"
#include "Prefab.h"
#include "Vector3.h"
#include "Quaternion.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <stdexcept>

// rapidjson includes
#include "../ThirdParty/rapidjson-master/include/rapidjson/document.h"
#include "../ThirdParty/rapidjson-master/include/rapidjson/error/en.h"

using namespace rapidjson;

//============================================================================
// MAIN LOADING ALGORITHM
//============================================================================

Scene* SceneLoader::LoadFromFile(const std::string& filePath) {
    std::cout << "[SceneLoader] ==== LOADING SCENE (rapidjson) ====" << std::endl;
    std::cout << "[SceneLoader] File: " << filePath << std::endl;

    try {
        std::string fileContents = ReadFileToString(filePath);
        if (fileContents.empty()) {
            throw std::runtime_error("File is empty or could not be read");
        }

        Document root;
        root.Parse(fileContents.c_str());
        if (root.HasParseError()) {
            throw std::runtime_error(std::string("JSON parse error: ") + GetParseError_En(root.GetParseError()));
        }
        std::cout << "[SceneLoader] JSON parsed successfully" << std::endl;

        if (root.HasMember("version") && root["version"].IsInt()) {
            int version = root["version"].GetInt();
            if (version != 1) {
                std::cerr << "[SceneLoader] WARNING: Loading unsupported scene version: " << version << std::endl;
            }
        } else {
            std::cerr << "[SceneLoader] WARNING: No scene version found in file." << std::endl;
        }

        std::string sceneName = root.HasMember("name") && root["name"].IsString() ? root["name"].GetString() : "UntitledScene";

        Scene* scene = new Scene(sceneName);
        scene->SetFilePath(filePath);
        std::cout << "[SceneLoader] Created scene: " << sceneName << std::endl;

        if (root.HasMember("metadata") && root["metadata"].IsObject()) {
            ApplySceneMetadata(scene, root["metadata"]);
        }

        std::vector<std::shared_ptr<SceneObject>> allObjects;
        if (root.HasMember("objects") && root["objects"].IsArray()) {
            const Value& objects = root["objects"];
            std::cout << "[SceneLoader] Loading " << objects.Size() << " objects..." << std::endl;

            for (const auto& objData : objects.GetArray()) {
                std::shared_ptr<SceneObject> sceneObject = nullptr;
                if (objData.HasMember("prefab") && objData["prefab"].IsString()) {
                    sceneObject = LoadPrefabInsideScene(objData, scene);
                } else {
                    sceneObject = CreateObjectFromData(objData, scene);
                }

                if (sceneObject) {
                    scene->AddSceneObject(sceneObject);
                    allObjects.push_back(sceneObject);
                }
            }
        }

        if (root.HasMember("hierarchy") && root["hierarchy"].IsArray()) {
            BuildHierarchy(allObjects, root["hierarchy"]);
        }

        std::cout << "[SceneLoader] ==== SCENE LOADED SUCCESSFULLY ====" << std::endl;
        return scene;

    } catch (const std::exception& e) {
        std::cerr << "[SceneLoader] ERROR: " << e.what() << std::endl;
        return nullptr;
    }
}

//============================================================================
// PREFAB LOADING
//============================================================================

std::shared_ptr<SceneObject> SceneLoader::LoadPrefabInsideScene(const Value& fileData, Scene* scene) {
    try {
        std::string prefabPath = fileData["prefab"].GetString();
        Prefab* prefab = PrefabRegistry::Get().Get(prefabPath);
        if (!prefab) {
            throw std::runtime_error("Prefab not found: " + prefabPath);
        }

        std::shared_ptr<SceneObject> instance = prefab->Instantiate();
        if (!instance) {
            throw std::runtime_error("Prefab instantiation failed");
        }

        if (fileData.HasMember("transform") && fileData["transform"].IsObject()) {
            const Value& transform = fileData["transform"];
            if (transform.HasMember("position") && transform["position"].IsObject()) {
                const Value& pos = transform["position"];
                instance->transform.SetPosition(::Vector3(pos["x"].GetFloat(), pos["y"].GetFloat(), pos["z"].GetFloat()));
            }
            if (transform.HasMember("rotation") && transform["rotation"].IsObject()) {
                const Value& rot = transform["rotation"];
                instance->transform.SetRotation(::Quaternion(rot["x"].GetFloat(), rot["y"].GetFloat(), rot["z"].GetFloat(), rot["w"].GetFloat()));
            }
            if (transform.HasMember("scale") && transform["scale"].IsObject()) {
                const Value& scale = transform["scale"];
                instance->transform.SetScale(::Vector3(scale["x"].GetFloat(), scale["y"].GetFloat(), scale["z"].GetFloat()));
            }
        }

        return instance;
    } catch (const std::exception& e) {
        std::cerr << "[SceneLoader] ERROR loading prefab: " << e.what() << std::endl;
        return nullptr;
    }
}

//============================================================================
// OBJECT CREATION
//============================================================================

std::shared_ptr<SceneObject> SceneLoader::CreateObjectFromData(const Value& objectData, Scene* scene) {
    std::string name = objectData.HasMember("name") && objectData["name"].IsString() ? objectData["name"].GetString() : "Unnamed";
    auto sceneObject = std::make_shared<SceneObject>(name);

    if (objectData.HasMember("transform") && objectData["transform"].IsObject()) {
        const Value& transform = objectData["transform"];
        if (transform.HasMember("position") && transform["position"].IsObject()) {
            const Value& pos = transform["position"];
            sceneObject->transform.SetPosition(::Vector3(pos["x"].GetFloat(), pos["y"].GetFloat(), pos["z"].GetFloat()));
        }
        if (transform.HasMember("rotation") && transform["rotation"].IsObject()) {
            const Value& rot = transform["rotation"];
            sceneObject->transform.SetRotation(::Quaternion(rot["x"].GetFloat(), rot["y"].GetFloat(), rot["z"].GetFloat(), rot["w"].GetFloat()));
        }
        if (transform.HasMember("scale") && transform["scale"].IsObject()) {
            const Value& scale = transform["scale"];
            sceneObject->transform.SetScale(::Vector3(scale["x"].GetFloat(), scale["y"].GetFloat(), scale["z"].GetFloat()));
        }
    }

    if (objectData.HasMember("components") && objectData["components"].IsArray()) {
        for (const auto& comp : objectData["components"].GetArray()) {
            if (comp.HasMember("type") && comp["type"].IsString()) {
                std::string compType = comp["type"].GetString();
                if (compType == "RenderableMesh" && comp.HasMember("meshAssetHandle")) {
                    sceneObject->SetRenderableMesh(AssetHandle(comp["meshAssetHandle"].GetUint64()));
                } else if (compType == "Material" && comp.HasMember("materialAssetHandle")) {
                    sceneObject->SetMaterial(AssetHandle(comp["materialAssetHandle"].GetUint64()));
                } else if (compType == "StaticBody") {
                    StaticBodyComponent sbc;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) sbc.enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("collisionLayer") && comp["collisionLayer"].IsUint()) sbc.collisionLayer = comp["collisionLayer"].GetUint();
                    if (comp.HasMember("collisionMask") && comp["collisionMask"].IsUint()) sbc.collisionMask = comp["collisionMask"].GetUint();
                    sceneObject->SetStaticBody(sbc);
                } else if (compType == "BoxCollider") {
                    BoxColliderComponent bcc;
                    if (comp.HasMember("size") && comp["size"].IsObject()) {
                        const auto& sizeObj = comp["size"];
                        if (sizeObj.HasMember("x") && sizeObj["x"].IsNumber()) bcc.size.x = sizeObj["x"].GetFloat();
                        if (sizeObj.HasMember("y") && sizeObj["y"].IsNumber()) bcc.size.y = sizeObj["y"].GetFloat();
                        if (sizeObj.HasMember("z") && sizeObj["z"].IsNumber()) bcc.size.z = sizeObj["z"].GetFloat();
                    }
                    if (comp.HasMember("offset") && comp["offset"].IsObject()) {
                        const auto& offsetObj = comp["offset"];
                        if (offsetObj.HasMember("x") && offsetObj["x"].IsNumber()) bcc.offset.x = offsetObj["x"].GetFloat();
                        if (offsetObj.HasMember("y") && offsetObj["y"].IsNumber()) bcc.offset.y = offsetObj["y"].GetFloat();
                        if (offsetObj.HasMember("z") && offsetObj["z"].IsNumber()) bcc.offset.z = offsetObj["z"].GetFloat();
                    }
                    if (comp.HasMember("isTrigger") && comp["isTrigger"].IsBool()) bcc.isTrigger = comp["isTrigger"].GetBool();
                    if (comp.HasMember("debugDraw") && comp["debugDraw"].IsBool()) bcc.debugDraw = comp["debugDraw"].GetBool();
                    sceneObject->SetBoxCollider(bcc);
                } else if (compType == "SphereCollider") {
                    SphereColliderComponent scc;
                    if (comp.HasMember("radius") && comp["radius"].IsNumber()) scc.radius = comp["radius"].GetFloat();
                    if (comp.HasMember("offset") && comp["offset"].IsObject()) {
                        const auto& offsetObj = comp["offset"];
                        if (offsetObj.HasMember("x") && offsetObj["x"].IsNumber()) scc.offset.x = offsetObj["x"].GetFloat();
                        if (offsetObj.HasMember("y") && offsetObj["y"].IsNumber()) scc.offset.y = offsetObj["y"].GetFloat();
                        if (offsetObj.HasMember("z") && offsetObj["z"].IsNumber()) scc.offset.z = offsetObj["z"].GetFloat();
                    }
                    if (comp.HasMember("isTrigger") && comp["isTrigger"].IsBool()) scc.isTrigger = comp["isTrigger"].GetBool();
                    if (comp.HasMember("debugDraw") && comp["debugDraw"].IsBool()) scc.debugDraw = comp["debugDraw"].GetBool();
                    sceneObject->SetSphereCollider(scc);
                } else if (compType == "CapsuleCollider") {
                    CapsuleColliderComponent ccc;
                    if (comp.HasMember("radius") && comp["radius"].IsNumber()) ccc.radius = comp["radius"].GetFloat();
                    if (comp.HasMember("height") && comp["height"].IsNumber()) ccc.height = comp["height"].GetFloat();
                    if (comp.HasMember("offset") && comp["offset"].IsObject()) {
                        const auto& offsetObj = comp["offset"];
                        if (offsetObj.HasMember("x") && offsetObj["x"].IsNumber()) ccc.offset.x = offsetObj["x"].GetFloat();
                        if (offsetObj.HasMember("y") && offsetObj["y"].IsNumber()) ccc.offset.y = offsetObj["y"].GetFloat();
                        if (offsetObj.HasMember("z") && offsetObj["z"].IsNumber()) ccc.offset.z = offsetObj["z"].GetFloat();
                    }
                    if (comp.HasMember("isTrigger") && comp["isTrigger"].IsBool()) ccc.isTrigger = comp["isTrigger"].GetBool();
                    if (comp.HasMember("debugDraw") && comp["debugDraw"].IsBool()) ccc.debugDraw = comp["debugDraw"].GetBool();
                    sceneObject->SetCapsuleCollider(ccc);
                }
            }
        }
    }

    return sceneObject;
}

//============================================================================
// HIERARCHY & METADATA
//============================================================================

void SceneLoader::BuildHierarchy(std::vector<std::shared_ptr<SceneObject>>& objects, const Value& hierarchyData) {
    std::unordered_map<std::string, std::shared_ptr<SceneObject>> objectMap;
    for (auto& obj : objects) {
        objectMap[obj->GetName()] = obj;
    }

    for (const auto& entry : hierarchyData.GetArray()) {
        if (entry.HasMember("parent") && entry["parent"].IsString() && entry.HasMember("child") && entry["child"].IsString()) {
            auto parentIt = objectMap.find(entry["parent"].GetString());
            auto childIt = objectMap.find(entry["child"].GetString());
            if (parentIt != objectMap.end() && childIt != objectMap.end()) {
                parentIt->second->AddChild(childIt->second.get());
                childIt->second->SetParent(parentIt->second.get());
            }
        }
    }
}

void SceneLoader::ApplySceneMetadata(Scene* scene, const Value& metadata) {
    // Placeholder
}

void SceneLoader::ApplyComponentOverrides(std::shared_ptr<SceneObject> object, const Value& overrides) {
    // Placeholder
}

void SceneLoader::BuildPrefabChildren(std::shared_ptr<SceneObject> parent, const Value& childrenData, Scene* scene) {
    // Placeholder
}

//============================================================================
// UTILITIES
//============================================================================

std::string SceneLoader::ReadFileToString(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filePath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool SceneLoader::ValidateSceneFile(const std::string& filePath) {
    return true; // Placeholder
}

std::vector<std::string> SceneLoader::GetSupportedExtensions() {
    return { ".json", ".omnixscene" };
}