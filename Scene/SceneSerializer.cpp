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