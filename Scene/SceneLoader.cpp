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
#include <algorithm>
 
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
 
        scene->RebuildRootObjects();

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
                    if (comp.HasMember("isTrigger") && comp["isTrigger"].IsBool()) ccc.isTrigger = ccc.isTrigger = comp["isTrigger"].GetBool();
                    if (comp.HasMember("debugDraw") && comp["debugDraw"].IsBool()) ccc.debugDraw = comp["debugDraw"].GetBool();
                    sceneObject->SetCapsuleCollider(ccc);
                } else if (compType == "PlayerStart") {
                    PlayerStartComponent psc;
                    if (comp.HasMember("active") && comp["active"].IsBool()) psc.active = comp["active"].GetBool();
                    sceneObject->SetPlayerStart(psc);
                } else if (compType == "CharacterController") {
                    CharacterControllerComponent cc;
                    if (comp.HasMember("moveSpeed") && comp["moveSpeed"].IsNumber()) cc.moveSpeed = comp["moveSpeed"].GetFloat();
                    if (comp.HasMember("sprintSpeed") && comp["sprintSpeed"].IsNumber()) cc.sprintSpeed = comp["sprintSpeed"].GetFloat();
                    if (comp.HasMember("mouseSensitivity") && comp["mouseSensitivity"].IsNumber()) cc.mouseSensitivity = comp["mouseSensitivity"].GetFloat();
                    if (comp.HasMember("gravity") && comp["gravity"].IsNumber()) cc.gravity = comp["gravity"].GetFloat();
                    if (comp.HasMember("jumpVelocity") && comp["jumpVelocity"].IsNumber()) cc.jumpVelocity = comp["jumpVelocity"].GetFloat();
                    if (comp.HasMember("capsuleRadius") && comp["capsuleRadius"].IsNumber()) cc.capsuleRadius = comp["capsuleRadius"].GetFloat();
                    if (comp.HasMember("capsuleHeight") && comp["capsuleHeight"].IsNumber()) cc.capsuleHeight = comp["capsuleHeight"].GetFloat();
                    if (comp.HasMember("groundCheckDistance") && comp["groundCheckDistance"].IsNumber()) cc.groundCheckDistance = comp["groundCheckDistance"].GetFloat();
                    if (comp.HasMember("skinWidth") && comp["skinWidth"].IsNumber()) cc.skinWidth = comp["skinWidth"].GetFloat();
                    if (comp.HasMember("enableJump") && comp["enableJump"].IsBool()) cc.enableJump = comp["enableJump"].GetBool();
                    sceneObject->SetCharacterController(cc);
                } else if (compType == "Camera") {
                    CameraComponent cam;
                    if (comp.HasMember("fov") && comp["fov"].IsNumber()) cam.fov = comp["fov"].GetFloat();
                    if (comp.HasMember("nearPlane") && comp["nearPlane"].IsNumber()) cam.nearPlane = comp["nearPlane"].GetFloat();
                    if (comp.HasMember("farPlane") && comp["farPlane"].IsNumber()) cam.farPlane = comp["farPlane"].GetFloat();
                    if (comp.HasMember("isPrimary") && comp["isPrimary"].IsBool()) cam.isPrimary = comp["isPrimary"].GetBool();
                    if (comp.HasMember("exposure") && comp["exposure"].IsNumber()) cam.exposure = comp["exposure"].GetFloat();
                    if (comp.HasMember("localOffset") && comp["localOffset"].IsObject()) {
                        const auto& lo = comp["localOffset"];
                        if (lo.HasMember("x") && lo["x"].IsNumber()) cam.localOffset.x = lo["x"].GetFloat();
                        if (lo.HasMember("y") && lo["y"].IsNumber()) cam.localOffset.y = lo["y"].GetFloat();
                        if (lo.HasMember("z") && lo["z"].IsNumber()) cam.localOffset.z = lo["z"].GetFloat();
                    }
                    sceneObject->SetCameraComponent(cam);
                } else if (compType == "Input") {
                    InputComponent ic;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) ic.enabled = comp["enabled"].GetBool();
                    sceneObject->SetInputComponent(ic);
                } else if (compType == "Trigger") {
                    TriggerComponent tc;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) tc.enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("shapeType") && comp["shapeType"].IsString()) {
                        std::string shape = comp["shapeType"].GetString();
                        if (shape == "Box") tc.shapeType = TriggerShapeType::Box;
                        else if (shape == "Sphere") tc.shapeType = TriggerShapeType::Sphere;
                        else if (shape == "Capsule") tc.shapeType = TriggerShapeType::Capsule;
                    }
                    if (comp.HasMember("boxSize") && comp["boxSize"].IsArray() && comp["boxSize"].Size() == 3) {
                        tc.boxSize.x = std::max(0.01f, comp["boxSize"][0].GetFloat());
                        tc.boxSize.y = std::max(0.01f, comp["boxSize"][1].GetFloat());
                        tc.boxSize.z = std::max(0.01f, comp["boxSize"][2].GetFloat());
                    }
                    if (comp.HasMember("sphereRadius") && comp["sphereRadius"].IsNumber()) {
                        tc.sphereRadius = std::max(0.01f, comp["sphereRadius"].GetFloat());
                    }
                    if (comp.HasMember("capsuleRadius") && comp["capsuleRadius"].IsNumber()) {
                        tc.capsuleRadius = std::max(0.01f, comp["capsuleRadius"].GetFloat());
                    }
                    if (comp.HasMember("capsuleHeight") && comp["capsuleHeight"].IsNumber()) {
                        tc.capsuleHeight = std::max(0.01f, comp["capsuleHeight"].GetFloat());
                    }
                    if (comp.HasMember("offset") && comp["offset"].IsArray() && comp["offset"].Size() == 3) {
                        tc.offset.x = comp["offset"][0].GetFloat();
                        tc.offset.y = comp["offset"][1].GetFloat();
                        tc.offset.z = comp["offset"][2].GetFloat();
                    }
                    if (comp.HasMember("eventName") && comp["eventName"].IsString()) tc.eventName = comp["eventName"].GetString();
                    if (comp.HasMember("fireEnter") && comp["fireEnter"].IsBool()) tc.fireEnter = comp["fireEnter"].GetBool();
                    if (comp.HasMember("fireStay") && comp["fireStay"].IsBool()) tc.fireStay = comp["fireStay"].GetBool();
                    if (comp.HasMember("fireExit") && comp["fireExit"].IsBool()) tc.fireExit = comp["fireExit"].GetBool();
 
                    // Validation clamping
                    if (!std::isfinite(tc.boxSize.x)) tc.boxSize.x = 1.0f;
                    if (!std::isfinite(tc.boxSize.y)) tc.boxSize.y = 1.0f;
                    if (!std::isfinite(tc.boxSize.z)) tc.boxSize.z = 1.0f;
                    if (!std::isfinite(tc.sphereRadius)) tc.sphereRadius = 0.5f;
                    if (!std::isfinite(tc.capsuleRadius)) tc.capsuleRadius = 0.5f;
                    if (!std::isfinite(tc.capsuleHeight)) tc.capsuleHeight = 2.0f;
                    if (!std::isfinite(tc.offset.x)) tc.offset.x = 0.0f;
                    if (!std::isfinite(tc.offset.y)) tc.offset.y = 0.0f;
                    if (!std::isfinite(tc.offset.z)) tc.offset.z = 0.0f;
 
                    sceneObject->SetTrigger(tc);
                } else if (compType == "Interactable") {
                    InteractableComponent ic;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) ic.Enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("promptText") && comp["promptText"].IsString()) ic.PromptText = comp["promptText"].GetString();
                    else if (comp.HasMember("interactionName") && comp["interactionName"].IsString()) ic.PromptText = comp["interactionName"].GetString();
                    if (comp.HasMember("interactionRadius") && comp["interactionRadius"].IsNumber()) {
                        ic.InteractionRadius = std::max(0.1f, comp["interactionRadius"].GetFloat());
                    }
                    if (comp.HasMember("interactionType") && comp["interactionType"].IsInt()) {
                        ic.Type = static_cast<InteractionType>(std::clamp(comp["interactionType"].GetInt(), 0, 6));
                    }
                    sceneObject->SetInteractable(ic);
                } else if (compType == "AudioSource") {
                    AudioSourceComponent asc;
                    if (comp.HasMember("clipPath") && comp["clipPath"].IsString()) asc.ClipPath = comp["clipPath"].GetString();
                    if (comp.HasMember("playOnStart") && comp["playOnStart"].IsBool()) asc.PlayOnStart = comp["playOnStart"].GetBool();
                    if (comp.HasMember("loop") && comp["loop"].IsBool()) asc.Loop = comp["loop"].GetBool();
                    if (comp.HasMember("volume") && comp["volume"].IsNumber()) {
                        asc.Volume = std::clamp(comp["volume"].GetFloat(), 0.0f, 1.0f);
                    }
                    if (comp.HasMember("isPlaying") && comp["isPlaying"].IsBool()) asc.IsPlaying = comp["isPlaying"].GetBool();
                    sceneObject->SetAudioSource(asc);
                } else if (compType == "Objective") {
                    ObjectiveComponent oc;
                    if (comp.HasMember("objectiveID") && comp["objectiveID"].IsString()) oc.ObjectiveID = comp["objectiveID"].GetString();
                    if (comp.HasMember("title") && comp["title"].IsString()) oc.Title = comp["title"].GetString();
                    if (comp.HasMember("description") && comp["description"].IsString()) oc.Description = comp["description"].GetString();
                    if (comp.HasMember("completionMode") && comp["completionMode"].IsInt()) {
                        oc.CompletionMode = static_cast<ObjectiveCompletionMode>(std::clamp(comp["completionMode"].GetInt(), 0, 2));
                    }
                    if (comp.HasMember("startsActive") && comp["startsActive"].IsBool()) oc.StartsActive = comp["startsActive"].GetBool();
                    if (comp.HasMember("repeatable") && comp["repeatable"].IsBool()) oc.Repeatable = comp["repeatable"].GetBool();
                    if (comp.HasMember("completed") && comp["completed"].IsBool()) oc.Completed = comp["completed"].GetBool();
                    sceneObject->SetObjective(oc);
                } else if (compType == "SimpleState") {
                    SimpleStateComponent ssc;
                    if (comp.HasMember("initialState") && comp["initialState"].IsInt()) {
                        ssc.InitialState = static_cast<SimpleObjectState>(std::clamp(comp["initialState"].GetInt(), 0, 4));
                    }
                    ssc.CurrentState = ssc.InitialState;
                    if (comp.HasMember("currentState") && comp["currentState"].IsInt()) {
                        ssc.CurrentState = static_cast<SimpleObjectState>(std::clamp(comp["currentState"].GetInt(), 0, 4));
                    }
                    if (comp.HasMember("resetOnPlay") && comp["resetOnPlay"].IsBool()) ssc.ResetOnPlay = comp["resetOnPlay"].GetBool();
                    sceneObject->SetSimpleState(ssc);
                } else if (compType == "Activatable") {
                    ActivatableComponent ac;
                    if (comp.HasMember("activationID") && comp["activationID"].IsString()) ac.ActivationID = comp["activationID"].GetString();
                    if (comp.HasMember("targetActivationID") && comp["targetActivationID"].IsString()) ac.TargetActivationID = comp["targetActivationID"].GetString();
                    if (comp.HasMember("requiresUnlocked") && comp["requiresUnlocked"].IsBool()) ac.RequiresUnlocked = comp["requiresUnlocked"].GetBool();
                    if (comp.HasMember("oneShot") && comp["oneShot"].IsBool()) ac.OneShot = comp["oneShot"].GetBool();
                    if (comp.HasMember("hasActivated") && comp["hasActivated"].IsBool()) ac.HasActivated = comp["hasActivated"].GetBool();
                    sceneObject->SetActivatable(ac);
                } else if (compType == "Door") {
                    DoorComponent dc;
                    if (comp.HasMember("closedPosition") && comp["closedPosition"].IsArray() && comp["closedPosition"].Size() == 3) {
                        dc.ClosedPosition.x = comp["closedPosition"][0].GetFloat();
                        dc.ClosedPosition.y = comp["closedPosition"][1].GetFloat();
                        dc.ClosedPosition.z = comp["closedPosition"][2].GetFloat();
                    }
                    if (comp.HasMember("openOffset") && comp["openOffset"].IsArray() && comp["openOffset"].Size() == 3) {
                        dc.OpenOffset.x = comp["openOffset"][0].GetFloat();
                        dc.OpenOffset.y = comp["openOffset"][1].GetFloat();
                        dc.OpenOffset.z = comp["openOffset"][2].GetFloat();
                    }
                    if (comp.HasMember("openSpeed") && comp["openSpeed"].IsNumber()) dc.OpenSpeed = comp["openSpeed"].GetFloat();
                    if (comp.HasMember("openMode") && comp["openMode"].IsInt()) {
                        dc.OpenMode = static_cast<DoorOpenMode>(std::clamp(comp["openMode"].GetInt(), 0, 1));
                    }
                    if (comp.HasMember("isOpen") && comp["isOpen"].IsBool()) dc.IsOpen = comp["isOpen"].GetBool();
                    sceneObject->SetDoor(dc);
                } else if (compType == "Checkpoint") {
                    CheckpointComponent cc;
                    if (comp.HasMember("checkpointID") && comp["checkpointID"].IsString()) cc.CheckpointID = comp["checkpointID"].GetString();
                    if (comp.HasMember("checkpointName") && comp["checkpointName"].IsString()) cc.CheckpointName = comp["checkpointName"].GetString();
                    if (comp.HasMember("activateOnTriggerEnter") && comp["activateOnTriggerEnter"].IsBool()) cc.ActivateOnTriggerEnter = comp["activateOnTriggerEnter"].GetBool();
                    if (comp.HasMember("oneShot") && comp["oneShot"].IsBool()) cc.OneShot = comp["oneShot"].GetBool();
                    // HasActivated always defaults to false on scene load
                    cc.HasActivated = false;
                    sceneObject->SetCheckpoint(cc);
                } else if (compType == "DirectionalLight") {
                    DirectionalLightComponent dlc;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) dlc.enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("color") && comp["color"].IsObject()) {
                        const auto& colorObj = comp["color"];
                        if (colorObj.HasMember("x") && colorObj["x"].IsNumber()) dlc.color.x = std::clamp(colorObj["x"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("y") && colorObj["y"].IsNumber()) dlc.color.y = std::clamp(colorObj["y"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("z") && colorObj["z"].IsNumber()) dlc.color.z = std::clamp(colorObj["z"].GetFloat(), 0.0f, 1.0f);
                    }
                    if (comp.HasMember("intensity") && comp["intensity"].IsNumber()) dlc.intensity = std::max(0.0f, comp["intensity"].GetFloat());
                    if (comp.HasMember("castShadows") && comp["castShadows"].IsBool()) dlc.castShadows = comp["castShadows"].GetBool();
                    if (comp.HasMember("shadowBias") && comp["shadowBias"].IsNumber()) dlc.shadowBias = comp["shadowBias"].GetFloat();
                    if (comp.HasMember("shadowSlopeBias") && comp["shadowSlopeBias"].IsNumber()) dlc.shadowSlopeBias = comp["shadowSlopeBias"].GetFloat();
                    if (comp.HasMember("shadowNormalBias") && comp["shadowNormalBias"].IsNumber()) dlc.shadowNormalBias = comp["shadowNormalBias"].GetFloat();
                    if (comp.HasMember("shadowStrength") && comp["shadowStrength"].IsNumber()) dlc.shadowStrength = comp["shadowStrength"].GetFloat();
                    if (comp.HasMember("shadowResolution") && comp["shadowResolution"].IsInt()) dlc.shadowResolution = comp["shadowResolution"].GetInt();
                    if (comp.HasMember("pcfKernelSize") && comp["pcfKernelSize"].IsInt()) dlc.pcfKernelSize = comp["pcfKernelSize"].GetInt();
                    if (comp.HasMember("shadowDistance") && comp["shadowDistance"].IsNumber()) dlc.shadowDistance = comp["shadowDistance"].GetFloat();
                    if (comp.HasMember("temperature") && comp["temperature"].IsNumber()) dlc.temperature = comp["temperature"].GetFloat();
                    if (comp.HasMember("layerMask") && comp["layerMask"].IsUint()) dlc.layerMask = comp["layerMask"].GetUint();
                    sceneObject->SetDirectionalLight(dlc);
                } else if (compType == "PointLight") {
                    PointLightComponent plc;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) plc.enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("color") && comp["color"].IsObject()) {
                        const auto& colorObj = comp["color"];
                        if (colorObj.HasMember("x") && colorObj["x"].IsNumber()) plc.color.x = std::clamp(colorObj["x"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("y") && colorObj["y"].IsNumber()) plc.color.y = std::clamp(colorObj["y"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("z") && colorObj["z"].IsNumber()) plc.color.z = std::clamp(colorObj["z"].GetFloat(), 0.0f, 1.0f);
                    }
                    if (comp.HasMember("intensity") && comp["intensity"].IsNumber()) plc.intensity = std::max(0.0f, comp["intensity"].GetFloat());
                    if (comp.HasMember("radius") && comp["radius"].IsNumber()) plc.radius = std::max(0.01f, comp["radius"].GetFloat());
                    if (comp.HasMember("castShadows") && comp["castShadows"].IsBool()) plc.castShadows = comp["castShadows"].GetBool();
                    if (comp.HasMember("temperature") && comp["temperature"].IsNumber()) plc.temperature = comp["temperature"].GetFloat();
                    if (comp.HasMember("layerMask") && comp["layerMask"].IsUint()) plc.layerMask = comp["layerMask"].GetUint();
                    if (comp.HasMember("sourceRadius") && comp["sourceRadius"].IsNumber()) plc.sourceRadius = comp["sourceRadius"].GetFloat();
                    sceneObject->SetPointLight(plc);
                } else if (compType == "SkyLight" || compType == "AmbientLight") {
                    SkyLightComponent slc;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) slc.enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("color") && comp["color"].IsObject()) {
                        const auto& colorObj = comp["color"];
                        if (colorObj.HasMember("x") && colorObj["x"].IsNumber()) slc.color.x = std::clamp(colorObj["x"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("y") && colorObj["y"].IsNumber()) slc.color.y = std::clamp(colorObj["y"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("z") && colorObj["z"].IsNumber()) slc.color.z = std::clamp(colorObj["z"].GetFloat(), 0.0f, 1.0f);
                    }
                    if (comp.HasMember("intensity") && comp["intensity"].IsNumber()) slc.intensity = std::max(0.0f, comp["intensity"].GetFloat());
                    if (comp.HasMember("environmentPath") && comp["environmentPath"].IsString()) slc.environmentPath = comp["environmentPath"].GetString();
                    if (comp.HasMember("rotation") && comp["rotation"].IsNumber()) slc.rotation = comp["rotation"].GetFloat();
                    if (comp.HasMember("diffuseIntensity") && comp["diffuseIntensity"].IsNumber()) slc.diffuseIntensity = comp["diffuseIntensity"].GetFloat();
                    if (comp.HasMember("specularIntensity") && comp["specularIntensity"].IsNumber()) slc.specularIntensity = comp["specularIntensity"].GetFloat();
                    if (comp.HasMember("exposureOffset") && comp["exposureOffset"].IsNumber()) slc.exposureOffset = comp["exposureOffset"].GetFloat();
                    if (comp.HasMember("mode") && comp["mode"].IsInt()) slc.mode = comp["mode"].GetInt();
                    sceneObject->SetSkyLight(slc);
                } else if (compType == "SpotLight") {
                    SpotLightComponent slc;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) slc.enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("color") && comp["color"].IsObject()) {
                        const auto& colorObj = comp["color"];
                        if (colorObj.HasMember("x") && colorObj["x"].IsNumber()) slc.color.x = std::clamp(colorObj["x"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("y") && colorObj["y"].IsNumber()) slc.color.y = std::clamp(colorObj["y"].GetFloat(), 0.0f, 1.0f);
                        if (colorObj.HasMember("z") && colorObj["z"].IsNumber()) slc.color.z = std::clamp(colorObj["z"].GetFloat(), 0.0f, 1.0f);
                    }
                    if (comp.HasMember("intensity") && comp["intensity"].IsNumber()) slc.intensity = std::max(0.0f, comp["intensity"].GetFloat());
                    if (comp.HasMember("range") && comp["range"].IsNumber()) slc.range = std::max(0.01f, comp["range"].GetFloat());
                    if (comp.HasMember("innerConeAngle") && comp["innerConeAngle"].IsNumber()) slc.innerConeAngle = comp["innerConeAngle"].GetFloat();
                    if (comp.HasMember("outerConeAngle") && comp["outerConeAngle"].IsNumber()) slc.outerConeAngle = comp["outerConeAngle"].GetFloat();
                    if (comp.HasMember("castShadows") && comp["castShadows"].IsBool()) slc.castShadows = comp["castShadows"].GetBool();
                    if (comp.HasMember("temperature") && comp["temperature"].IsNumber()) slc.temperature = comp["temperature"].GetFloat();
                    if (comp.HasMember("layerMask") && comp["layerMask"].IsUint()) slc.layerMask = comp["layerMask"].GetUint();
                    if (comp.HasMember("sourceRadius") && comp["sourceRadius"].IsNumber()) slc.sourceRadius = comp["sourceRadius"].GetFloat();
                    sceneObject->SetSpotLight(slc);
                } else if (compType == "ReflectionProbe") {
                    ReflectionProbeComponent rpc;
                    if (comp.HasMember("enabled") && comp["enabled"].IsBool()) rpc.enabled = comp["enabled"].GetBool();
                    if (comp.HasMember("position") && comp["position"].IsObject()) {
                        const auto& posObj = comp["position"];
                        if (posObj.HasMember("x") && posObj["x"].IsNumber()) rpc.position.x = posObj["x"].GetFloat();
                        if (posObj.HasMember("y") && posObj["y"].IsNumber()) rpc.position.y = posObj["y"].GetFloat();
                        if (posObj.HasMember("z") && posObj["z"].IsNumber()) rpc.position.z = posObj["z"].GetFloat();
                    }
                    if (comp.HasMember("boxMin") && comp["boxMin"].IsObject()) {
                        const auto& minObj = comp["boxMin"];
                        if (minObj.HasMember("x") && minObj["x"].IsNumber()) rpc.boxMin.x = minObj["x"].GetFloat();
                        if (minObj.HasMember("y") && minObj["y"].IsNumber()) rpc.boxMin.y = minObj["y"].GetFloat();
                        if (minObj.HasMember("z") && minObj["z"].IsNumber()) rpc.boxMin.z = minObj["z"].GetFloat();
                    }
                    if (comp.HasMember("boxMax") && comp["boxMax"].IsObject()) {
                        const auto& maxObj = comp["boxMax"];
                        if (maxObj.HasMember("x") && maxObj["x"].IsNumber()) rpc.boxMax.x = maxObj["x"].GetFloat();
                        if (maxObj.HasMember("y") && maxObj["y"].IsNumber()) rpc.boxMax.y = maxObj["y"].GetFloat();
                        if (maxObj.HasMember("z") && maxObj["z"].IsNumber()) rpc.boxMax.z = maxObj["z"].GetFloat();
                    }
                    if (comp.HasMember("blendDistance") && comp["blendDistance"].IsNumber()) rpc.blendDistance = comp["blendDistance"].GetFloat();
                    if (comp.HasMember("intensity") && comp["intensity"].IsNumber()) rpc.intensity = comp["intensity"].GetFloat();
                    if (comp.HasMember("priority") && comp["priority"].IsUint()) rpc.priority = comp["priority"].GetUint();
                    if (comp.HasMember("isBox") && comp["isBox"].IsBool()) rpc.isBox = comp["isBox"].GetBool();
                    if (comp.HasMember("capturePath") && comp["capturePath"].IsString()) rpc.capturePath = comp["capturePath"].GetString();
                    sceneObject->SetReflectionProbe(rpc);
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