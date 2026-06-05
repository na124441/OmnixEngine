#include "SceneValidator.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Scene/PrefabRegistry.h"
#include "../ThirdParty/rapidjson-master/include/rapidjson/document.h"
#include "../ThirdParty/rapidjson-master/include/rapidjson/error/en.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <iostream>

using namespace rapidjson;

SceneValidationReport SceneValidator::ValidateSceneFile(
    const std::string& path,
    const eng::runtime::AssetRegistry* assetRegistry,
    const PrefabRegistry* prefabRegistry
) {
    SceneValidationReport report;
    std::ifstream file(path);
    if (!file.is_open()) {
        report.AddFatal("SCENE_FILE_NOT_FOUND", "Could not open scene file: " + path);
        return report;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    if (content.empty()) {
        report.AddFatal("SCENE_FILE_EMPTY", "Scene file is empty: " + path);
        return report;
    }

    Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
        std::string parseError = GetParseError_En(doc.GetParseError());
        report.AddFatal("SCENE_JSON_PARSE_ERROR", "JSON parse error: " + parseError, "offset: " + std::to_string(doc.GetErrorOffset()));
        return report;
    }

    return ValidateSceneDocument(doc, assetRegistry, prefabRegistry);
}

SceneValidationReport SceneValidator::ValidateSceneDocument(
    const Document& doc,
    const eng::runtime::AssetRegistry* assetRegistry,
    const PrefabRegistry* prefabRegistry
) {
    SceneValidationReport report;

    // -------------------------------------------------------------------------
    // 1. Metadata and Header Checks
    // -------------------------------------------------------------------------
    if (doc.HasMember("format")) {
        if (!doc["format"].IsString()) {
            report.AddError("SCENE_INVALID_FORMAT_TYPE", "Scene 'format' field must be a string");
        } else {
            std::string formatStr = doc["format"].GetString();
            if (formatStr != "OmnixScene") {
                report.AddWarning("SCENE_UNEXPECTED_FORMAT", "Unexpected scene format identifier: " + formatStr);
            }
        }
    }

    if (doc.HasMember("version")) {
        if (!doc["version"].IsInt()) {
            report.AddError("SCENE_INVALID_VERSION_TYPE", "Scene 'version' field must be an integer");
        } else {
            int version = doc["version"].GetInt();
            if (version < 1) {
                report.AddError("SCENE_VERSION_TOO_LOW", "Scene version " + std::to_string(version) + " is not supported");
            } else if (version > 1) {
                report.AddError("SCENE_VERSION_UNSUPPORTED", "Scene version " + std::to_string(version) + " is newer than supported version 1");
            }
        }
    } else {
        report.AddWarning("SCENE_MISSING_VERSION", "No scene version specified, defaulting to version 1 behavior");
    }

    if (doc.HasMember("name")) {
        if (!doc["name"].IsString()) {
            report.AddWarning("SCENE_INVALID_NAME_TYPE", "Scene name must be a string");
        }
    } else {
        report.AddInfo("SCENE_MISSING_NAME", "Scene name not specified");
    }

    if (!doc.HasMember("objects") || !doc["objects"].IsArray()) {
        report.AddFatal("SCENE_MISSING_OBJECTS", "Scene is missing 'objects' array or it is not valid");
        return report;
    }

    const auto& objects = doc["objects"].GetArray();
    std::unordered_set<std::string> objectNames;
    std::unordered_map<std::string, bool> hasTransformMap;
    int playerStartCount = 0;

    // -------------------------------------------------------------------------
    // 2. Objects and Components Validation
    // -------------------------------------------------------------------------
    int objIdx = 0;
    for (const auto& obj : objects) {
        std::string entityName = "Unnamed";
        if (obj.HasMember("name") && obj["name"].IsString()) {
            entityName = obj["name"].GetString();
        } else {
            report.AddWarning("SCENE_UNNAMED_OBJECT", "Object at index " + std::to_string(objIdx) + " is unnamed", "objects[" + std::to_string(objIdx) + "]");
        }

        // Check uniqueness of name (acts as ID)
        if (objectNames.find(entityName) != objectNames.end()) {
            report.AddError("SCENE_DUPLICATE_ENTITY_NAME", "Duplicate object name detected: '" + entityName + "'. Entity names must be unique.", "objects[" + std::to_string(objIdx) + "]", entityName);
        } else {
            objectNames.insert(entityName);
        }

        // Prefab Reference Checks
        if (obj.HasMember("prefab")) {
            if (!obj["prefab"].IsString()) {
                report.AddError("SCENE_INVALID_PREFAB_PATH", "Prefab path must be a string", "objects[" + std::to_string(objIdx) + "]", entityName);
            } else {
                std::string prefabPath = obj["prefab"].GetString();
                std::ifstream prefabFile(prefabPath);
                if (!prefabFile.is_open()) {
                    report.AddError("SCENE_PREFAB_NOT_FOUND", "Prefab file not found: " + prefabPath, "objects[" + std::to_string(objIdx) + "]", entityName);
                }
            }
        }

        // Transform Block Validation
        bool hasTransform = false;
        if (obj.HasMember("transform")) {
            if (!obj["transform"].IsObject()) {
                report.AddError("SCENE_INVALID_TRANSFORM_BLOCK", "Transform must be an object", "objects[" + std::to_string(objIdx) + "]", entityName);
            } else {
                hasTransform = true;
                const auto& transform = obj["transform"];
                
                // Position
                if (transform.HasMember("position")) {
                    const auto& pos = transform["position"];
                    if (!pos.IsObject() || !pos.HasMember("x") || !pos.HasMember("y") || !pos.HasMember("z") ||
                        !pos["x"].IsNumber() || !pos["y"].IsNumber() || !pos["z"].IsNumber()) {
                        report.AddError("SCENE_INVALID_POSITION", "Position must have numeric x, y, and z", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                    } else {
                        float x = pos["x"].GetFloat();
                        float y = pos["y"].GetFloat();
                        float z = pos["z"].GetFloat();
                        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                            report.AddError("SCENE_NAN_POSITION", "Position coordinates must be finite", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                        } else if (std::abs(x) > 100000.0f || std::abs(y) > 100000.0f || std::abs(z) > 100000.0f) {
                            report.AddWarning("SCENE_LARGE_POSITION", "Position coordinates are extremely large", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                        }
                    }
                } else {
                    report.AddError("SCENE_MISSING_POSITION", "Transform is missing position", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                }
                
                // Rotation
                if (transform.HasMember("rotation")) {
                    const auto& rot = transform["rotation"];
                    if (!rot.IsObject() || !rot.HasMember("x") || !rot.HasMember("y") || !rot.HasMember("z") || !rot.HasMember("w") ||
                        !rot["x"].IsNumber() || !rot["y"].IsNumber() || !rot["z"].IsNumber() || !rot["w"].IsNumber()) {
                        report.AddError("SCENE_INVALID_ROTATION", "Rotation must be a quaternion with numeric x, y, z, and w", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                    } else {
                        float x = rot["x"].GetFloat();
                        float y = rot["y"].GetFloat();
                        float z = rot["z"].GetFloat();
                        float w = rot["w"].GetFloat();
                        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w)) {
                            report.AddError("SCENE_NAN_ROTATION", "Rotation coordinates must be finite", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                        } else {
                            float lenSq = x*x + y*y + z*z + w*w;
                            if (lenSq < 0.0001f) {
                                report.AddError("SCENE_ZERO_ROTATION_LENGTH", "Rotation quaternion length cannot be zero", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                            }
                        }
                    }
                } else {
                    report.AddError("SCENE_MISSING_ROTATION", "Transform is missing rotation", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                }

                // Scale
                if (transform.HasMember("scale")) {
                    const auto& scale = transform["scale"];
                    if (!scale.IsObject() || !scale.HasMember("x") || !scale.HasMember("y") || !scale.HasMember("z") ||
                        !scale["x"].IsNumber() || !scale["y"].IsNumber() || !scale["z"].IsNumber()) {
                        report.AddError("SCENE_INVALID_SCALE", "Scale must have numeric x, y, and z", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                    } else {
                        float x = scale["x"].GetFloat();
                        float y = scale["y"].GetFloat();
                        float z = scale["z"].GetFloat();
                        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                            report.AddError("SCENE_NAN_SCALE", "Scale coordinates must be finite", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                        } else if (x <= 0.0f || y <= 0.0f || z <= 0.0f) {
                            report.AddError("SCENE_ZERO_OR_NEGATIVE_SCALE", "Scale must be positive (> 0)", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                        } else if (x < 0.001f || y < 0.001f || z < 0.001f) {
                            report.AddWarning("SCENE_EXTREMELY_SMALL_SCALE", "Scale is extremely small (clamped to 0.001 internally)", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                        }
                    }
                } else {
                    report.AddError("SCENE_MISSING_SCALE", "Transform is missing scale", "objects[" + std::to_string(objIdx) + "].transform", entityName);
                }
            }
        }
        hasTransformMap[entityName] = hasTransform;

        // Component Validation
        if (obj.HasMember("components") && obj["components"].IsArray()) {
            int compIdx = 0;
            for (const auto& comp : obj["components"].GetArray()) {
                if (!comp.IsObject() || !comp.HasMember("type") || !comp["type"].IsString()) {
                    report.AddError("SCENE_INVALID_COMPONENT", "Component must be an object with a 'type' string", "objects[" + std::to_string(objIdx) + "].components[" + std::to_string(compIdx) + "]", entityName);
                    compIdx++;
                    continue;
                }

                std::string compType = comp["type"].GetString();
                std::string compPath = "objects[" + std::to_string(objIdx) + "].components[" + std::to_string(compIdx) + "(" + compType + ")]";

                // Require transform for physical entities and meshes
                if (!hasTransform && (compType == "RenderableMesh" || compType == "BoxCollider" || 
                                     compType == "SphereCollider" || compType == "CapsuleCollider" || 
                                     compType == "Trigger" || compType == "PlayerStart")) {
                    report.AddError("SCENE_PHYSICAL_ENTITY_WITHOUT_TRANSFORM", "Component '" + compType + "' requires the entity to have a valid transform", compPath, entityName);
                }

                if (compType == "RenderableMesh") {
                    if (comp.HasMember("meshAssetHandle") && comp["meshAssetHandle"].IsUint64()) {
                        uint64_t handle = comp["meshAssetHandle"].GetUint64();
                        if (handle == 0) {
                            report.AddError("SCENE_NULL_MESH_HANDLE", "Mesh asset handle cannot be zero", compPath, entityName);
                        } else if (assetRegistry) {
                            const auto* meta = assetRegistry->GetMetadata(AssetHandle(handle));
                            if (!meta) {
                                report.AddError("SCENE_INVALID_MESH_HANDLE", "Mesh asset handle " + std::to_string(handle) + " not found in AssetRegistry", compPath, entityName);
                            } else if (meta->type != AssetType::Mesh) {
                                report.AddError("SCENE_WRONG_ASSET_TYPE", "Asset type mismatch: handle " + std::to_string(handle) + " is of type '" + AssetTypeToString(meta->type) + "' but expected 'Mesh'", compPath, entityName);
                            }
                        }
                    } else {
                        report.AddError("SCENE_MISSING_MESH_HANDLE", "RenderableMesh component is missing 'meshAssetHandle'", compPath, entityName);
                    }
                }
                else if (compType == "Material") {
                    if (comp.HasMember("materialAssetHandle") && comp["materialAssetHandle"].IsUint64()) {
                        uint64_t handle = comp["materialAssetHandle"].GetUint64();
                        if (handle == 0) {
                            report.AddError("SCENE_NULL_MATERIAL_HANDLE", "Material asset handle cannot be zero", compPath, entityName);
                        } else if (assetRegistry) {
                            const auto* meta = assetRegistry->GetMetadata(AssetHandle(handle));
                            if (!meta) {
                                report.AddError("SCENE_INVALID_MATERIAL_HANDLE", "Material asset handle " + std::to_string(handle) + " not found in AssetRegistry", compPath, entityName);
                            } else if (meta->type != AssetType::Material) {
                                report.AddError("SCENE_WRONG_ASSET_TYPE", "Asset type mismatch: handle " + std::to_string(handle) + " is of type '" + AssetTypeToString(meta->type) + "' but expected 'Material'", compPath, entityName);
                            }
                        }
                    } else {
                        report.AddError("SCENE_MISSING_MATERIAL_HANDLE", "Material component is missing 'materialAssetHandle'", compPath, entityName);
                    }
                }
                else if (compType == "BoxCollider") {
                    if (comp.HasMember("size") && comp["size"].IsObject()) {
                        const auto& sizeObj = comp["size"];
                        if (sizeObj.HasMember("x") && sizeObj["x"].IsNumber() &&
                            sizeObj.HasMember("y") && sizeObj["y"].IsNumber() &&
                            sizeObj.HasMember("z") && sizeObj["z"].IsNumber()) {
                            float sx = sizeObj["x"].GetFloat();
                            float sy = sizeObj["y"].GetFloat();
                            float sz = sizeObj["z"].GetFloat();
                            if (sx <= 0.0f || sy <= 0.0f || sz <= 0.0f) {
                                report.AddError("SCENE_INVALID_BOX_SIZE", "Box collider dimensions must be positive", compPath, entityName);
                            }
                        } else {
                            report.AddError("SCENE_INVALID_BOX_SIZE_TYPE", "Box collider size must contain numeric x, y, and z", compPath, entityName);
                        }
                    } else {
                        report.AddError("SCENE_MISSING_BOX_SIZE", "Box collider is missing 'size'", compPath, entityName);
                    }
                }
                else if (compType == "SphereCollider") {
                    if (comp.HasMember("radius") && comp["radius"].IsNumber()) {
                        float r = comp["radius"].GetFloat();
                        if (r <= 0.0f) {
                            report.AddError("SCENE_INVALID_SPHERE_RADIUS", "Sphere collider radius must be positive", compPath, entityName);
                        }
                    } else {
                        report.AddError("SCENE_MISSING_SPHERE_RADIUS", "Sphere collider is missing or has invalid 'radius'", compPath, entityName);
                    }
                }
                else if (compType == "CapsuleCollider") {
                    bool hasR = comp.HasMember("radius") && comp["radius"].IsNumber();
                    bool hasH = comp.HasMember("height") && comp["height"].IsNumber();
                    if (hasR && hasH) {
                        float r = comp["radius"].GetFloat();
                        float h = comp["height"].GetFloat();
                        if (r <= 0.0f || h <= 0.0f) {
                            report.AddError("SCENE_INVALID_CAPSULE_DIMENSIONS", "Capsule dimensions must be positive", compPath, entityName);
                        } else if (h < 2.0f * r) {
                            report.AddError("SCENE_INVALID_CAPSULE_HEIGHT", "Capsule height (" + std::to_string(h) + ") must be at least twice the radius (" + std::to_string(2.0f * r) + ")", compPath, entityName);
                        }
                    } else {
                        report.AddError("SCENE_MISSING_CAPSULE_DIMENSIONS", "Capsule collider must specify numeric 'radius' and 'height'", compPath, entityName);
                    }
                }
                else if (compType == "Trigger") {
                    if (comp.HasMember("shapeType") && comp["shapeType"].IsString()) {
                        std::string shape = comp["shapeType"].GetString();
                        if (shape != "Box" && shape != "Sphere" && shape != "Capsule") {
                            report.AddError("SCENE_INVALID_TRIGGER_SHAPE", "Trigger shapeType must be 'Box', 'Sphere', or 'Capsule'", compPath, entityName);
                        }
                    } else {
                        report.AddError("SCENE_MISSING_TRIGGER_SHAPE", "Trigger is missing 'shapeType'", compPath, entityName);
                    }
                    if (comp.HasMember("eventName")) {
                        if (!comp["eventName"].IsString()) {
                            report.AddError("SCENE_INVALID_TRIGGER_EVENT", "Trigger eventName must be a string", compPath, entityName);
                        } else if (std::string(comp["eventName"].GetString()).empty()) {
                            report.AddWarning("SCENE_EMPTY_TRIGGER_EVENT", "Trigger has an empty event name", compPath, entityName);
                        }
                    }
                }
                else if (compType == "DirectionalLight" || compType == "PointLight" || compType == "AmbientLight" || compType == "SpotLight") {
                    if (comp.HasMember("color") && comp["color"].IsObject()) {
                        const auto& col = comp["color"];
                        if (col.HasMember("x") && col["x"].IsNumber() &&
                            col.HasMember("y") && col["y"].IsNumber() &&
                            col.HasMember("z") && col["z"].IsNumber()) {
                            float cx = col["x"].GetFloat();
                            float cy = col["y"].GetFloat();
                            float cz = col["z"].GetFloat();
                            if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz)) {
                                report.AddError("SCENE_NAN_LIGHT_COLOR", "Light color must have finite values", compPath, entityName);
                            } else if (cx < 0.0f || cx > 1.0f || cy < 0.0f || cy > 1.0f || cz < 0.0f || cz > 1.0f) {
                                report.AddWarning("SCENE_CLAMPED_LIGHT_COLOR", "Light color components should be in range [0, 1] (will be clamped)", compPath, entityName);
                            }
                        } else {
                            report.AddError("SCENE_INVALID_LIGHT_COLOR", "Light color must have numeric x, y, and z", compPath, entityName);
                        }
                    }
                    if (comp.HasMember("intensity") && comp["intensity"].IsNumber()) {
                        float intensity = comp["intensity"].GetFloat();
                        if (!std::isfinite(intensity) || intensity < 0.0f) {
                            report.AddError("SCENE_INVALID_LIGHT_INTENSITY", "Light intensity must be a non-negative finite value", compPath, entityName);
                        }
                    }
                    if (compType == "PointLight") {
                        if (comp.HasMember("radius") && comp["radius"].IsNumber()) {
                            float radius = comp["radius"].GetFloat();
                            if (radius <= 0.0f || !std::isfinite(radius)) {
                                report.AddError("SCENE_INVALID_LIGHT_RADIUS", "Point light radius must be positive", compPath, entityName);
                            }
                        } else {
                            report.AddError("SCENE_MISSING_LIGHT_RADIUS", "Point light is missing 'radius'", compPath, entityName);
                        }
                    }
                    if (compType == "SpotLight") {
                        if (comp.HasMember("range") && comp["range"].IsNumber()) {
                            float r = comp["range"].GetFloat();
                            if (r <= 0.0f || !std::isfinite(r)) {
                                report.AddError("SCENE_INVALID_LIGHT_RANGE", "Spot light range must be positive", compPath, entityName);
                            }
                        }
                        if (comp.HasMember("innerConeAngle") && comp["innerConeAngle"].IsNumber() &&
                            comp.HasMember("outerConeAngle") && comp["outerConeAngle"].IsNumber()) {
                            float inner = comp["innerConeAngle"].GetFloat();
                            float outer = comp["outerConeAngle"].GetFloat();
                            if (inner < 0.0f || outer <= inner) {
                                report.AddError("SCENE_INVALID_SPOT_ANGLES", "Spot light angles must satisfy: innerConeAngle >= 0 and outerConeAngle > innerConeAngle", compPath, entityName);
                            }
                        }
                    }
                }
                else if (compType == "PlayerStart") {
                    playerStartCount++;
                }

                compIdx++;
            }
        }

        objIdx++;
    }

    if (playerStartCount == 0) {
        report.AddWarning("SCENE_NO_PLAYER_START", "No PlayerStart entities defined in scene. Players may spawn at default origin.");
    } else if (playerStartCount > 1) {
        report.AddWarning("SCENE_MULTIPLE_PLAYER_STARTS", "Multiple PlayerStart entities (" + std::to_string(playerStartCount) + ") defined. Only the first active one will be used.");
    }

    // -------------------------------------------------------------------------
    // 3. Hierarchy Validation (Cycles, Missing Parents/Children)
    // -------------------------------------------------------------------------
    if (doc.HasMember("hierarchy") && doc["hierarchy"].IsArray()) {
        const auto& hierarchy = doc["hierarchy"].GetArray();
        std::unordered_map<std::string, std::vector<std::string>> graph;
        std::unordered_map<std::string, std::string> childToParent;

        int entryIdx = 0;
        for (const auto& entry : hierarchy) {
            std::string path = "hierarchy[" + std::to_string(entryIdx) + "]";
            if (!entry.IsObject() || !entry.HasMember("parent") || !entry.HasMember("child") ||
                !entry["parent"].IsString() || !entry["child"].IsString()) {
                report.AddError("SCENE_INVALID_HIERARCHY_ENTRY", "Hierarchy entry must contain parent and child strings", path);
                entryIdx++;
                continue;
            }

            std::string parent = entry["parent"].GetString();
            std::string child = entry["child"].GetString();

            if (objectNames.find(parent) == objectNames.end()) {
                report.AddError("SCENE_HIERARCHY_MISSING_PARENT", "Parent object '" + parent + "' not found in objects list", path);
            }
            if (objectNames.find(child) == objectNames.end()) {
                report.AddError("SCENE_HIERARCHY_MISSING_CHILD", "Child object '" + child + "' not found in objects list", path);
            }

            if (parent == child) {
                report.AddError("SCENE_HIERARCHY_SELF_PARENT", "Object '" + parent + "' cannot be its own parent", path);
            }

            // Check if child already has a parent
            if (childToParent.find(child) != childToParent.end()) {
                report.AddError("SCENE_HIERARCHY_DUPLICATE_PARENT", "Child object '" + child + "' is assigned to multiple parents: '" + childToParent[child] + "' and '" + parent + "'", path);
            } else {
                childToParent[child] = parent;
            }

            graph[parent].push_back(child);
            entryIdx++;
        }

        // Cycle Detection using DFS
        std::unordered_map<std::string, int> visitState; // 0 = unvisited, 1 = visiting, 2 = visited
        std::function<bool(const std::string&)> hasCycle = [&](const std::string& node) {
            visitState[node] = 1; // visiting
            for (const auto& child : graph[node]) {
                if (visitState[child] == 1) {
                    return true;
                } else if (visitState[child] == 0) {
                    if (hasCycle(child)) return true;
                }
            }
            visitState[node] = 2; // visited
            return false;
        };

        for (const auto& name : objectNames) {
            if (visitState[name] == 0) {
                if (hasCycle(name)) {
                    report.AddError("SCENE_HIERARCHY_CYCLE", "Hierarchy contains a parent-child cycle involving entity '" + name + "'", "hierarchy");
                    break; 
                }
            }
        }
    }

    return report;
}
