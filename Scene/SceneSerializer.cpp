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
        if (object->HasRenderableMesh()) {
            Value comp(kObjectType);
            comp.AddMember("type", "RenderableMesh", allocator);
            comp.AddMember("meshAssetHandle", object->GetMeshAssetHandle().value, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasMaterial()) {
            Value comp(kObjectType);
            comp.AddMember("type", "Material", allocator);
            comp.AddMember("materialAssetHandle", object->GetMaterialAssetHandle().value, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasStaticBody()) {
            const auto& sb = object->GetStaticBody();
            Value comp(kObjectType);
            comp.AddMember("type", "StaticBody", allocator);
            comp.AddMember("enabled", sb.enabled, allocator);
            comp.AddMember("collisionLayer", sb.collisionLayer, allocator);
            comp.AddMember("collisionMask", sb.collisionMask, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasBoxCollider()) {
            const auto& bc = object->GetBoxCollider();
            Value comp(kObjectType);
            comp.AddMember("type", "BoxCollider", allocator);
            
            Value sizeVal(kObjectType);
            sizeVal.AddMember("x", bc.size.x, allocator);
            sizeVal.AddMember("y", bc.size.y, allocator);
            sizeVal.AddMember("z", bc.size.z, allocator);
            comp.AddMember("size", sizeVal, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", bc.offset.x, allocator);
            offsetVal.AddMember("y", bc.offset.y, allocator);
            offsetVal.AddMember("z", bc.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("isTrigger", bc.isTrigger, allocator);
            comp.AddMember("debugDraw", bc.debugDraw, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasSphereCollider()) {
            const auto& sc = object->GetSphereCollider();
            Value comp(kObjectType);
            comp.AddMember("type", "SphereCollider", allocator);
            comp.AddMember("radius", sc.radius, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", sc.offset.x, allocator);
            offsetVal.AddMember("y", sc.offset.y, allocator);
            offsetVal.AddMember("z", sc.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("isTrigger", sc.isTrigger, allocator);
            comp.AddMember("debugDraw", sc.debugDraw, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasCapsuleCollider()) {
            const auto& cc = object->GetCapsuleCollider();
            Value comp(kObjectType);
            comp.AddMember("type", "CapsuleCollider", allocator);
            comp.AddMember("radius", cc.radius, allocator);
            comp.AddMember("height", cc.height, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", cc.offset.x, allocator);
            offsetVal.AddMember("y", cc.offset.y, allocator);
            offsetVal.AddMember("z", cc.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("isTrigger", cc.isTrigger, allocator);
            comp.AddMember("debugDraw", cc.debugDraw, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasPlayerStart()) {
            const auto& ps = object->GetPlayerStart();
            Value comp(kObjectType);
            comp.AddMember("type", "PlayerStart", allocator);
            comp.AddMember("active", ps.active, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasCharacterController()) {
            const auto& cc = object->GetCharacterController();
            Value comp(kObjectType);
            comp.AddMember("type", "CharacterController", allocator);
            comp.AddMember("moveSpeed", cc.moveSpeed, allocator);
            comp.AddMember("sprintSpeed", cc.sprintSpeed, allocator);
            comp.AddMember("mouseSensitivity", cc.mouseSensitivity, allocator);
            comp.AddMember("gravity", cc.gravity, allocator);
            comp.AddMember("jumpVelocity", cc.jumpVelocity, allocator);
            comp.AddMember("capsuleRadius", cc.capsuleRadius, allocator);
            comp.AddMember("capsuleHeight", cc.capsuleHeight, allocator);
            comp.AddMember("groundCheckDistance", cc.groundCheckDistance, allocator);
            comp.AddMember("skinWidth", cc.skinWidth, allocator);
            comp.AddMember("enableJump", cc.enableJump, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasCameraComponent()) {
            const auto& cam = object->GetCameraComponent();
            Value comp(kObjectType);
            comp.AddMember("type", "Camera", allocator);
            comp.AddMember("fov", cam.fov, allocator);
            comp.AddMember("nearPlane", cam.nearPlane, allocator);
            comp.AddMember("farPlane", cam.farPlane, allocator);
            comp.AddMember("isPrimary", cam.isPrimary, allocator);
            comp.AddMember("exposure", cam.exposure, allocator);

            Value offsetVal(kObjectType);
            offsetVal.AddMember("x", cam.localOffset.x, allocator);
            offsetVal.AddMember("y", cam.localOffset.y, allocator);
            offsetVal.AddMember("z", cam.localOffset.z, allocator);
            comp.AddMember("localOffset", offsetVal, allocator);

            components.PushBack(comp, allocator);
        }
        if (object->HasInputComponent()) {
            const auto& ic = object->GetInputComponent();
            Value comp(kObjectType);
            comp.AddMember("type", "Input", allocator);
            comp.AddMember("enabled", ic.enabled, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasTrigger()) {
            const auto& tr = object->GetTrigger();
            Value comp(kObjectType);
            comp.AddMember("type", "Trigger", allocator);
            comp.AddMember("enabled", tr.enabled, allocator);
            
            std::string shapeStr = "Box";
            if (tr.shapeType == TriggerShapeType::Sphere) shapeStr = "Sphere";
            else if (tr.shapeType == TriggerShapeType::Capsule) shapeStr = "Capsule";
            comp.AddMember("shapeType", Value(shapeStr.c_str(), allocator).Move(), allocator);

            Value boxSizeVal(kArrayType);
            boxSizeVal.PushBack(tr.boxSize.x, allocator);
            boxSizeVal.PushBack(tr.boxSize.y, allocator);
            boxSizeVal.PushBack(tr.boxSize.z, allocator);
            comp.AddMember("boxSize", boxSizeVal, allocator);

            comp.AddMember("sphereRadius", tr.sphereRadius, allocator);
            comp.AddMember("capsuleRadius", tr.capsuleRadius, allocator);
            comp.AddMember("capsuleHeight", tr.capsuleHeight, allocator);

            Value offsetVal(kArrayType);
            offsetVal.PushBack(tr.offset.x, allocator);
            offsetVal.PushBack(tr.offset.y, allocator);
            offsetVal.PushBack(tr.offset.z, allocator);
            comp.AddMember("offset", offsetVal, allocator);

            comp.AddMember("eventName", Value(tr.eventName.c_str(), allocator).Move(), allocator);
            comp.AddMember("fireEnter", tr.fireEnter, allocator);
            comp.AddMember("fireStay", tr.fireStay, allocator);
            comp.AddMember("fireExit", tr.fireExit, allocator);

            components.PushBack(comp, allocator);
        }
        if (object->HasInteractable()) {
            const auto& ia = object->GetInteractable();
            Value comp(kObjectType);
            comp.AddMember("type", "Interactable", allocator);
            comp.AddMember("enabled", ia.Enabled, allocator);
            comp.AddMember("promptText", Value(ia.PromptText.c_str(), allocator).Move(), allocator);
            comp.AddMember("interactionRadius", ia.InteractionRadius, allocator);
            comp.AddMember("interactionType", static_cast<int>(ia.Type), allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasAudioSource()) {
            const auto& as = object->GetAudioSource();
            Value comp(kObjectType);
            comp.AddMember("type", "AudioSource", allocator);
            comp.AddMember("clipPath", Value(as.ClipPath.c_str(), allocator).Move(), allocator);
            comp.AddMember("playOnStart", as.PlayOnStart, allocator);
            comp.AddMember("loop", as.Loop, allocator);
            comp.AddMember("volume", as.Volume, allocator);
            comp.AddMember("isPlaying", as.IsPlaying, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasObjective()) {
            const auto& ob = object->GetObjective();
            Value comp(kObjectType);
            comp.AddMember("type", "Objective", allocator);
            comp.AddMember("objectiveID", Value(ob.ObjectiveID.c_str(), allocator).Move(), allocator);
            comp.AddMember("title", Value(ob.Title.c_str(), allocator).Move(), allocator);
            comp.AddMember("description", Value(ob.Description.c_str(), allocator).Move(), allocator);
            comp.AddMember("completionMode", static_cast<int>(ob.CompletionMode), allocator);
            comp.AddMember("startsActive", ob.StartsActive, allocator);
            comp.AddMember("repeatable", ob.Repeatable, allocator);
            comp.AddMember("completed", false, allocator); // Reset on save/load
            components.PushBack(comp, allocator);
        }
        if (object->HasSimpleState()) {
            const auto& ss = object->GetSimpleState();
            Value comp(kObjectType);
            comp.AddMember("type", "SimpleState", allocator);
            comp.AddMember("initialState", static_cast<int>(ss.InitialState), allocator);
            comp.AddMember("currentState", static_cast<int>(ss.InitialState), allocator); // Current starts at initial
            comp.AddMember("resetOnPlay", ss.ResetOnPlay, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasActivatable()) {
            const auto& act = object->GetActivatable();
            Value comp(kObjectType);
            comp.AddMember("type", "Activatable", allocator);
            comp.AddMember("activationID", Value(act.ActivationID.c_str(), allocator).Move(), allocator);
            comp.AddMember("targetActivationID", Value(act.TargetActivationID.c_str(), allocator).Move(), allocator);
            comp.AddMember("requiresUnlocked", act.RequiresUnlocked, allocator);
            comp.AddMember("oneShot", act.OneShot, allocator);
            comp.AddMember("hasActivated", false, allocator); // Reset on save/load
            components.PushBack(comp, allocator);
        }
        if (object->HasDoor()) {
            const auto& dr = object->GetDoor();
            Value comp(kObjectType);
            comp.AddMember("type", "Door", allocator);
            
            Value closedPosVal(kArrayType);
            closedPosVal.PushBack(dr.ClosedPosition.x, allocator);
            closedPosVal.PushBack(dr.ClosedPosition.y, allocator);
            closedPosVal.PushBack(dr.ClosedPosition.z, allocator);
            comp.AddMember("closedPosition", closedPosVal, allocator);

            Value openOffsetVal(kArrayType);
            openOffsetVal.PushBack(dr.OpenOffset.x, allocator);
            openOffsetVal.PushBack(dr.OpenOffset.y, allocator);
            openOffsetVal.PushBack(dr.OpenOffset.z, allocator);
            comp.AddMember("openOffset", openOffsetVal, allocator);

            comp.AddMember("openSpeed", dr.OpenSpeed, allocator);
            comp.AddMember("openMode", static_cast<int>(dr.OpenMode), allocator);
            comp.AddMember("isOpen", false, allocator); // Reset on save/load
            components.PushBack(comp, allocator);
        }
        if (object->HasCheckpoint()) {
            const auto& cp = object->GetCheckpoint();
            Value comp(kObjectType);
            comp.AddMember("type", "Checkpoint", allocator);
            comp.AddMember("checkpointID", Value(cp.CheckpointID.c_str(), allocator).Move(), allocator);
            comp.AddMember("checkpointName", Value(cp.CheckpointName.c_str(), allocator).Move(), allocator);
            comp.AddMember("activateOnTriggerEnter", cp.ActivateOnTriggerEnter, allocator);
            comp.AddMember("oneShot", cp.OneShot, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasDirectionalLight()) {
            const auto& dl = object->GetDirectionalLight();
            Value comp(kObjectType);
            comp.AddMember("type", "DirectionalLight", allocator);
            comp.AddMember("enabled", dl.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", dl.color.x, allocator);
            colorVal.AddMember("y", dl.color.y, allocator);
            colorVal.AddMember("z", dl.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", dl.intensity, allocator);
            comp.AddMember("castShadows", dl.castShadows, allocator);
            comp.AddMember("shadowBias", dl.shadowBias, allocator);
            comp.AddMember("shadowSlopeBias", dl.shadowSlopeBias, allocator);
            comp.AddMember("shadowNormalBias", dl.shadowNormalBias, allocator);
            comp.AddMember("shadowStrength", dl.shadowStrength, allocator);
            comp.AddMember("shadowResolution", dl.shadowResolution, allocator);
            comp.AddMember("pcfKernelSize", dl.pcfKernelSize, allocator);
            comp.AddMember("shadowDistance", dl.shadowDistance, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasPointLight()) {
            const auto& pl = object->GetPointLight();
            Value comp(kObjectType);
            comp.AddMember("type", "PointLight", allocator);
            comp.AddMember("enabled", pl.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", pl.color.x, allocator);
            colorVal.AddMember("y", pl.color.y, allocator);
            colorVal.AddMember("z", pl.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", pl.intensity, allocator);
            comp.AddMember("radius", pl.radius, allocator);
            comp.AddMember("castShadows", pl.castShadows, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasSkyLight()) {
            const auto& sl = object->GetSkyLight();
            Value comp(kObjectType);
            comp.AddMember("type", "SkyLight", allocator);
            comp.AddMember("enabled", sl.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", sl.color.x, allocator);
            colorVal.AddMember("y", sl.color.y, allocator);
            colorVal.AddMember("z", sl.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", sl.intensity, allocator);
            components.PushBack(comp, allocator);
        }
        if (object->HasSpotLight()) {
            const auto& spl = object->GetSpotLight();
            Value comp(kObjectType);
            comp.AddMember("type", "SpotLight", allocator);
            comp.AddMember("enabled", spl.enabled, allocator);
            
            Value colorVal(kObjectType);
            colorVal.AddMember("x", spl.color.x, allocator);
            colorVal.AddMember("y", spl.color.y, allocator);
            colorVal.AddMember("z", spl.color.z, allocator);
            comp.AddMember("color", colorVal, allocator);
            
            comp.AddMember("intensity", spl.intensity, allocator);
            comp.AddMember("range", spl.range, allocator);
            comp.AddMember("innerConeAngle", spl.innerConeAngle, allocator);
            comp.AddMember("outerConeAngle", spl.outerConeAngle, allocator);
            comp.AddMember("castShadows", spl.castShadows, allocator);
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