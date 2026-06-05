//============================================================================
// SceneSerializer.cpp - Refactored to use rapidjson
//============================================================================

#include "SceneSerializer.h"
#include "Scene.h"
#include "SceneObject.h"
#include "Prefab.h"
#include "Transform.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <filesystem>

#include "../ThirdParty/rapidjson-master/include/rapidjson/document.h"
#include "../ThirdParty/rapidjson-master/include/rapidjson/writer.h"
#include "../ThirdParty/rapidjson-master/include/rapidjson/stringbuffer.h"
#include "../ThirdParty/rapidjson-master/include/rapidjson/prettywriter.h"

using namespace rapidjson;

bool SceneSerializer::SaveScene(Scene* scene, const std::string& filePath) {
    if (!scene) {
        return false;
    }

    Document doc;
    doc.SetObject();
    Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("name", Value(scene->GetName().c_str(), allocator).Move(), allocator);
    doc.AddMember("version", 1, allocator);

    // Serialize Flat list of objects
    Value objects(kArrayType);
    for (const auto& object : scene->GetAllSceneObjects()) {
        if (!object) continue;

        Value objValue(kObjectType);
        objValue.AddMember("name", Value(object->GetName().c_str(), allocator).Move(), allocator);

        // Serialize Transform Position, Rotation, Scale
        Value transformValue(kObjectType);
        
        Value pos(kObjectType);
        pos.AddMember("x", object->transform.GetPosition().x, allocator);
        pos.AddMember("y", object->transform.GetPosition().y, allocator);
        pos.AddMember("z", object->transform.GetPosition().z, allocator);
        transformValue.AddMember("position", pos, allocator);

        Value rot(kObjectType);
        rot.AddMember("x", object->transform.GetRotation().x, allocator);
        rot.AddMember("y", object->transform.GetRotation().y, allocator);
        rot.AddMember("z", object->transform.GetRotation().z, allocator);
        rot.AddMember("w", object->transform.GetRotation().w, allocator);
        transformValue.AddMember("rotation", rot, allocator);

        Value scale(kObjectType);
        scale.AddMember("x", object->transform.GetScale().x, allocator);
        scale.AddMember("y", object->transform.GetScale().y, allocator);
        scale.AddMember("z", object->transform.GetScale().z, allocator);
        transformValue.AddMember("scale", scale, allocator);

        objValue.AddMember("transform", transformValue, allocator);

        // Serialize Components
        Value components(kArrayType);
        if (object->m_HasRenderableMesh) {
            Value comp(kObjectType);
            comp.AddMember("type", "RenderableMesh", allocator);
            comp.AddMember("meshAssetHandle", object->m_MeshAssetHandle.value, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasMaterial) {
            Value comp(kObjectType);
            comp.AddMember("type", "Material", allocator);
            comp.AddMember("materialAssetHandle", object->m_MaterialAssetHandle.value, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasStaticBody) {
            Value comp(kObjectType);
            comp.AddMember("type", "StaticBody", allocator);
            comp.AddMember("enabled", object->m_StaticBody.enabled, allocator);
            comp.AddMember("collisionLayer", object->m_StaticBody.collisionLayer, allocator);
            comp.AddMember("collisionMask", object->m_StaticBody.collisionMask, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasBoxCollider) {
            Value comp(kObjectType);
            comp.AddMember("type", "BoxCollider", allocator);
            
            Value sizeVal(kObjectType);
            sizeVal.AddMember("x", object->m_BoxCollider.size.x, allocator);
            sizeVal.AddMember("y", object->m_BoxCollider.size.y, allocator);
            sizeVal.AddMember("z", object->m_BoxCollider.size.z, allocator);
            comp.AddMember("size", sizeVal, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", object->m_BoxCollider.offset.x, allocator);
            offsetVal.AddMember("y", object->m_BoxCollider.offset.y, allocator);
            offsetVal.AddMember("z", object->m_BoxCollider.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("isTrigger", object->m_BoxCollider.isTrigger, allocator);
            comp.AddMember("debugDraw", object->m_BoxCollider.debugDraw, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasSphereCollider) {
            Value comp(kObjectType);
            comp.AddMember("type", "SphereCollider", allocator);
            comp.AddMember("radius", object->m_SphereCollider.radius, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", object->m_SphereCollider.offset.x, allocator);
            offsetVal.AddMember("y", object->m_SphereCollider.offset.y, allocator);
            offsetVal.AddMember("z", object->m_SphereCollider.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("isTrigger", object->m_SphereCollider.isTrigger, allocator);
            comp.AddMember("debugDraw", object->m_SphereCollider.debugDraw, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasCapsuleCollider) {
            Value comp(kObjectType);
            comp.AddMember("type", "CapsuleCollider", allocator);
            comp.AddMember("radius", object->m_CapsuleCollider.radius, allocator);
            comp.AddMember("height", object->m_CapsuleCollider.height, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", object->m_CapsuleCollider.offset.x, allocator);
            offsetVal.AddMember("y", object->m_CapsuleCollider.offset.y, allocator);
            offsetVal.AddMember("z", object->m_CapsuleCollider.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("isTrigger", object->m_CapsuleCollider.isTrigger, allocator);
            comp.AddMember("debugDraw", object->m_CapsuleCollider.debugDraw, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasPlayerStart) {
            Value comp(kObjectType);
            comp.AddMember("type", "PlayerStart", allocator);
            comp.AddMember("active", object->m_PlayerStart.active, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasCharacterController) {
            Value comp(kObjectType);
            comp.AddMember("type", "CharacterController", allocator);
            comp.AddMember("moveSpeed", object->m_CharacterController.moveSpeed, allocator);
            comp.AddMember("sprintSpeed", object->m_CharacterController.sprintSpeed, allocator);
            comp.AddMember("mouseSensitivity", object->m_CharacterController.mouseSensitivity, allocator);
            comp.AddMember("gravity", object->m_CharacterController.gravity, allocator);
            comp.AddMember("jumpVelocity", object->m_CharacterController.jumpVelocity, allocator);
            comp.AddMember("capsuleRadius", object->m_CharacterController.capsuleRadius, allocator);
            comp.AddMember("capsuleHeight", object->m_CharacterController.capsuleHeight, allocator);
            comp.AddMember("groundCheckDistance", object->m_CharacterController.groundCheckDistance, allocator);
            comp.AddMember("skinWidth", object->m_CharacterController.skinWidth, allocator);
            comp.AddMember("enableJump", object->m_CharacterController.enableJump, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasCameraComponent) {
            Value comp(kObjectType);
            comp.AddMember("type", "Camera", allocator);
            comp.AddMember("fov", object->m_CameraComponent.fov, allocator);
            comp.AddMember("nearPlane", object->m_CameraComponent.nearPlane, allocator);
            comp.AddMember("farPlane", object->m_CameraComponent.farPlane, allocator);
            comp.AddMember("isPrimary", object->m_CameraComponent.isPrimary, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", object->m_CameraComponent.localOffset.x, allocator);
            offsetVal.AddMember("y", object->m_CameraComponent.localOffset.y, allocator);
            offsetVal.AddMember("z", object->m_CameraComponent.localOffset.z, allocator);
            comp.AddMember("localOffset", offsetVal, allocator);

            components.PushBack(comp, allocator);
        }
        if (object->m_HasInputComponent) {
            Value comp(kObjectType);
            comp.AddMember("type", "Input", allocator);
            comp.AddMember("enabled", object->m_InputComponent.enabled, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasTrigger) {
            Value comp(kObjectType);
            comp.AddMember("type", "Trigger", allocator);
            comp.AddMember("enabled", object->m_Trigger.enabled, allocator);
            
            std::string shapeStr = "Box";
            if (object->m_Trigger.shapeType == TriggerShapeType::Sphere) shapeStr = "Sphere";
            else if (object->m_Trigger.shapeType == TriggerShapeType::Capsule) shapeStr = "Capsule";
            comp.AddMember("shapeType", Value(shapeStr.c_str(), allocator).Move(), allocator);

            Value boxSizeVal(kArrayType);
            boxSizeVal.PushBack(object->m_Trigger.boxSize.x, allocator);
            boxSizeVal.PushBack(object->m_Trigger.boxSize.y, allocator);
            boxSizeVal.PushBack(object->m_Trigger.boxSize.z, allocator);
            comp.AddMember("boxSize", boxSizeVal, allocator);

            comp.AddMember("sphereRadius", object->m_Trigger.sphereRadius, allocator);
            comp.AddMember("capsuleRadius", object->m_Trigger.capsuleRadius, allocator);
            comp.AddMember("capsuleHeight", object->m_Trigger.capsuleHeight, allocator);

            Value offsetVal(kArrayType);
            offsetVal.PushBack(object->m_Trigger.offset.x, allocator);
            offsetVal.PushBack(object->m_Trigger.offset.y, allocator);
            offsetVal.PushBack(object->m_Trigger.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("eventName", Value(object->m_Trigger.eventName.c_str(), allocator).Move(), allocator);
            comp.AddMember("fireEnter", object->m_Trigger.fireEnter, allocator);
            comp.AddMember("fireStay", object->m_Trigger.fireStay, allocator);
            comp.AddMember("fireExit", object->m_Trigger.fireExit, allocator);

            components.PushBack(comp, allocator);
        }
        if (object->m_HasInteractable) {
            Value comp(kObjectType);
            comp.AddMember("type", "Interactable", allocator);
            comp.AddMember("enabled", object->m_Interactable.Enabled, allocator);
            comp.AddMember("promptText", Value(object->m_Interactable.PromptText.c_str(), allocator).Move(), allocator);
            comp.AddMember("interactionRadius", object->m_Interactable.InteractionRadius, allocator);
            comp.AddMember("interactionType", static_cast<int>(object->m_Interactable.Type), allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasAudioSource) {
            Value comp(kObjectType);
            comp.AddMember("type", "AudioSource", allocator);
            comp.AddMember("clipPath", Value(object->m_AudioSource.ClipPath.c_str(), allocator).Move(), allocator);
            comp.AddMember("playOnStart", object->m_AudioSource.PlayOnStart, allocator);
            comp.AddMember("loop", object->m_AudioSource.Loop, allocator);
            comp.AddMember("volume", object->m_AudioSource.Volume, allocator);
            comp.AddMember("isPlaying", object->m_AudioSource.IsPlaying, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasObjective) {
            Value comp(kObjectType);
            comp.AddMember("type", "Objective", allocator);
            comp.AddMember("objectiveID", Value(object->m_Objective.ObjectiveID.c_str(), allocator).Move(), allocator);
            comp.AddMember("title", Value(object->m_Objective.Title.c_str(), allocator).Move(), allocator);
            comp.AddMember("description", Value(object->m_Objective.Description.c_str(), allocator).Move(), allocator);
            comp.AddMember("completionMode", static_cast<int>(object->m_Objective.CompletionMode), allocator);
            comp.AddMember("startsActive", object->m_Objective.StartsActive, allocator);
            comp.AddMember("repeatable", object->m_Objective.Repeatable, allocator);
            comp.AddMember("completed", false, allocator); // Reset on save/load
            components.PushBack(comp, allocator);
        }
        if (object->m_HasSimpleState) {
            Value comp(kObjectType);
            comp.AddMember("type", "SimpleState", allocator);
            comp.AddMember("initialState", static_cast<int>(object->m_SimpleState.InitialState), allocator);
            comp.AddMember("currentState", static_cast<int>(object->m_SimpleState.InitialState), allocator); // Current starts at initial
            comp.AddMember("resetOnPlay", object->m_SimpleState.ResetOnPlay, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasActivatable) {
            Value comp(kObjectType);
            comp.AddMember("type", "Activatable", allocator);
            comp.AddMember("activationID", Value(object->m_Activatable.ActivationID.c_str(), allocator).Move(), allocator);
            comp.AddMember("targetActivationID", Value(object->m_Activatable.TargetActivationID.c_str(), allocator).Move(), allocator);
            comp.AddMember("requiresUnlocked", object->m_Activatable.RequiresUnlocked, allocator);
            comp.AddMember("oneShot", object->m_Activatable.OneShot, allocator);
            comp.AddMember("hasActivated", false, allocator); // Reset on save/load
            components.PushBack(comp, allocator);
        }
        if (object->m_HasDoor) {
            Value comp(kObjectType);
            comp.AddMember("type", "Door", allocator);
            
            Value closedPosVal(kArrayType);
            closedPosVal.PushBack(object->m_Door.ClosedPosition.x, allocator);
            closedPosVal.PushBack(object->m_Door.ClosedPosition.y, allocator);
            closedPosVal.PushBack(object->m_Door.ClosedPosition.z, allocator);
            comp.AddMember("closedPosition", closedPosVal, allocator);
 
            Value openOffsetVal(kArrayType);
            openOffsetVal.PushBack(object->m_Door.OpenOffset.x, allocator);
            openOffsetVal.PushBack(object->m_Door.OpenOffset.y, allocator);
            openOffsetVal.PushBack(object->m_Door.OpenOffset.z, allocator);
            comp.AddMember("openOffset", openOffsetVal, allocator);
 
            comp.AddMember("openSpeed", object->m_Door.OpenSpeed, allocator);
            comp.AddMember("openMode", static_cast<int>(object->m_Door.OpenMode), allocator);
            comp.AddMember("isOpen", false, allocator); // Reset on save/load
            components.PushBack(comp, allocator);
        }
        if (object->m_HasCheckpoint) {
            Value comp(kObjectType);
            comp.AddMember("type", "Checkpoint", allocator);
            comp.AddMember("checkpointID", Value(object->m_Checkpoint.CheckpointID.c_str(), allocator).Move(), allocator);
            comp.AddMember("checkpointName", Value(object->m_Checkpoint.CheckpointName.c_str(), allocator).Move(), allocator);
            comp.AddMember("activateOnTriggerEnter", object->m_Checkpoint.ActivateOnTriggerEnter, allocator);
            comp.AddMember("oneShot", object->m_Checkpoint.OneShot, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasDirectionalLight) {
            Value comp(kObjectType);
            comp.AddMember("type", "DirectionalLight", allocator);
            comp.AddMember("enabled", object->m_DirectionalLight.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", object->m_DirectionalLight.color.x, allocator);
            colorVal.AddMember("y", object->m_DirectionalLight.color.y, allocator);
            colorVal.AddMember("z", object->m_DirectionalLight.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", object->m_DirectionalLight.intensity, allocator);
            comp.AddMember("castShadows", object->m_DirectionalLight.castShadows, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasPointLight) {
            Value comp(kObjectType);
            comp.AddMember("type", "PointLight", allocator);
            comp.AddMember("enabled", object->m_PointLight.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", object->m_PointLight.color.x, allocator);
            colorVal.AddMember("y", object->m_PointLight.color.y, allocator);
            colorVal.AddMember("z", object->m_PointLight.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", object->m_PointLight.intensity, allocator);
            comp.AddMember("radius", object->m_PointLight.radius, allocator);
            comp.AddMember("castShadows", object->m_PointLight.castShadows, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasAmbientLight) {
            Value comp(kObjectType);
            comp.AddMember("type", "AmbientLight", allocator);
            comp.AddMember("enabled", object->m_AmbientLight.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", object->m_AmbientLight.color.x, allocator);
            colorVal.AddMember("y", object->m_AmbientLight.color.y, allocator);
            colorVal.AddMember("z", object->m_AmbientLight.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", object->m_AmbientLight.intensity, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->m_HasSpotLight) {
            Value comp(kObjectType);
            comp.AddMember("type", "SpotLight", allocator);
            comp.AddMember("enabled", object->m_SpotLight.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", object->m_SpotLight.color.x, allocator);
            colorVal.AddMember("y", object->m_SpotLight.color.y, allocator);
            colorVal.AddMember("z", object->m_SpotLight.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", object->m_SpotLight.intensity, allocator);
            comp.AddMember("range", object->m_SpotLight.range, allocator);
            comp.AddMember("innerConeAngle", object->m_SpotLight.innerConeAngle, allocator);
            comp.AddMember("outerConeAngle", object->m_SpotLight.outerConeAngle, allocator);
            comp.AddMember("castShadows", object->m_SpotLight.castShadows, allocator);
            components.PushBack(comp, allocator);
        }
        objValue.AddMember("components", components, allocator);

        objects.PushBack(objValue, allocator);
    }
    doc.AddMember("objects", objects, allocator);

    // Serialize Hierarchy
    Value hierarchy(kArrayType);
    for (const auto& object : scene->GetAllSceneObjects()) {
        if (object && object->GetParent()) {
            Value entry(kObjectType);
            entry.AddMember("parent", Value(object->GetParent()->GetName().c_str(), allocator).Move(), allocator);
            entry.AddMember("child", Value(object->GetName().c_str(), allocator).Move(), allocator);
            hierarchy.PushBack(entry, allocator);
        }
    }
    doc.AddMember("hierarchy", hierarchy, allocator);

    return WriteJSONToFile(doc, filePath);
}

bool SceneSerializer::SavePrefab(std::shared_ptr<Prefab> prefab, const std::string& filePath) {
    return false;
}

bool SceneSerializer::ValidateOutputPath(const std::string& filePath) {
    if (filePath.empty()) {
        return false;
    }
    auto extensions = GetSupportedExtensions();
    for (const auto& ext : extensions) {
        if (filePath.size() >= ext.size() &&
            filePath.compare(filePath.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> SceneSerializer::GetSupportedExtensions() {
    return { ".json", ".omnixscene" };
}

bool SceneSerializer::WriteJSONToFile(const Document& doc, const std::string& filePath, bool prettyPrint) {
    StringBuffer buffer;
    if (prettyPrint) {
        PrettyWriter<StringBuffer> writer(buffer);
        doc.Accept(writer);
    } else {
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);
    }

    try {
        std::filesystem::path path(filePath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (...) {}

    std::ofstream ofs(filePath);
    if (!ofs) {
        return false;
    }
    ofs << buffer.GetString();
    return true;
}

// Keep helper functions stubbed to avoid link-time errors
void SceneSerializer::SerializeSceneObject(rapidjson::Document& doc, std::shared_ptr<SceneObject> object, bool includeChildren) {}
void SceneSerializer::SerializeTransform(rapidjson::Value& transformValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> object) {}
void SceneSerializer::SerializeComponents(rapidjson::Value& componentsValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> object) {}
void SceneSerializer::SerializeHierarchy(rapidjson::Value& hierarchyValue, rapidjson::Document::AllocatorType& allocator, Scene* scene) {}
void SceneSerializer::SerializeSceneMetadata(rapidjson::Value& metadataValue, rapidjson::Document::AllocatorType& allocator, Scene* scene) {}
bool SceneSerializer::IsPrefabInstance(std::shared_ptr<SceneObject> object) { return false; }
std::string SceneSerializer::GetPrefabPath(std::shared_ptr<SceneObject> object) { return ""; }
void SceneSerializer::SerializePrefabOverrides(rapidjson::Value& overridesValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> object) {}
void SceneSerializer::SerializePrefabMetadata(rapidjson::Value& metadataValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<Prefab> prefab) {}
void SceneSerializer::SerializeChildren(rapidjson::Value& childrenValue, rapidjson::Document::AllocatorType& allocator, std::shared_ptr<SceneObject> parent) {}