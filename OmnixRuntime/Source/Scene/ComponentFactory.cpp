//============================================================================
// ComponentFactory.cpp - Implementation
//
// Component factory system with JSON deserialization
// Implements CreateFromJSON algorithm
//
// Created: November 25, 2025
//============================================================================

#include "Scene/ComponentFactory.h"
// #include "ECSCoordinator.h"  // TODO: Uncomment when ECS integrated

#include <iostream>
#include <sstream>

//============================================================================
// CORE ALGORITHM: CreateFromJSON
//============================================================================

/**
 * @brief CreateFromJSON - Create component from JSON data
 *
 * Algorithm (from specification):
 * 1. Read component type field
 * 2. Switch(type): TransformComponent, MeshRenderer, RigidBody, Script, etc.
 * 3. Allocate new component
 * 4. Apply JSON properties
 * 5. Register component into ECS storage under entityID
 * 6. Return created component
 */
Component* ComponentFactory::CreateFromJSON(const std::string& jsonData, uint32_t entityID) {
    std::cout << "[ComponentFactory] Creating component for entity " << entityID << std::endl;

    // STEP 1: Read component type field
    ComponentType type = ParseComponentType(jsonData);

    if (type == ComponentType::Unknown) {
        std::cerr << "[ComponentFactory] ERROR: Unknown component type in JSON" << std::endl;
        return nullptr;
    }

    std::cout << "[ComponentFactory] Component type: "
              << ComponentTypeToString(type) << std::endl;

    Component* component = nullptr;

    // STEP 2: Switch(type) - STEP 3: Allocate new component - STEP 4: Apply JSON properties
    switch (type) {
        case ComponentType::Transform:
            component = CreateTransformComponent(jsonData, entityID);
            break;

        case ComponentType::MeshRenderer:
            component = CreateMeshRenderer(jsonData, entityID);
            break;

        case ComponentType::RigidBody:
            component = CreateRigidBody(jsonData, entityID);
            break;

        case ComponentType::Script:
            component = CreateScript(jsonData, entityID);
            break;

        case ComponentType::Camera:
            component = CreateCameraComponent(jsonData, entityID);
            break;

        case ComponentType::Light:
            component = CreateLightComponent(jsonData, entityID);
            break;

        default:
            std::cerr << "[ComponentFactory] ERROR: Unhandled component type: "
                      << ComponentTypeToString(type) << std::endl;
            return nullptr;
    }

    if (!component) {
        std::cerr << "[ComponentFactory] ERROR: Failed to create component" << std::endl;
        return nullptr;
    }

    // STEP 5: Register component into ECS storage under entityID
    RegisterComponentWithECS(component, entityID);

    std::cout << "[ComponentFactory] Successfully created "
              << ComponentTypeToString(type)
              << " for entity " << entityID << std::endl;

    // STEP 6: Return created component
    return component;
}

//============================================================================
// TYPE PARSING
//============================================================================

ComponentFactory::ComponentType ComponentFactory::ParseComponentType(const std::string& jsonData) {
    // Extract "type" field from JSON
    std::string typeString = GetStringField(jsonData, "type");

    if (typeString.empty()) {
        std::cerr << "[ComponentFactory] ERROR: No 'type' field in JSON" << std::endl;
        return ComponentType::Unknown;
    }

    return StringToComponentType(typeString);
}

ComponentFactory::ComponentType ComponentFactory::StringToComponentType(const std::string& typeName) {
    if (typeName == "Transform") return ComponentType::Transform;
    if (typeName == "MeshRenderer") return ComponentType::MeshRenderer;
    if (typeName == "RigidBody") return ComponentType::RigidBody;
    if (typeName == "Script") return ComponentType::Script;
    if (typeName == "Camera") return ComponentType::Camera;
    if (typeName == "Light") return ComponentType::Light;
    if (typeName == "AudioSource") return ComponentType::AudioSource;
    if (typeName == "Collider") return ComponentType::Collider;
    if (typeName == "Animator") return ComponentType::Animator;

    return ComponentType::Unknown;
}

std::string ComponentFactory::ComponentTypeToString(ComponentType type) {
    switch (type) {
        case ComponentType::Transform: return "Transform";
        case ComponentType::MeshRenderer: return "MeshRenderer";
        case ComponentType::RigidBody: return "RigidBody";
        case ComponentType::Script: return "Script";
        case ComponentType::Camera: return "Camera";
        case ComponentType::Light: return "Light";
        case ComponentType::AudioSource: return "AudioSource";
        case ComponentType::Collider: return "Collider";
        case ComponentType::Animator: return "Animator";
        default: return "Unknown";
    }
}

//============================================================================
// COMPONENT CREATION HELPERS
//============================================================================

Component* ComponentFactory::CreateTransformComponent(const std::string& jsonData, uint32_t entityID) {
    std::cout << "[ComponentFactory]   Creating TransformComponent..." << std::endl;

    // TODO: Create actual TransformComponent class
    // For now, create base component as placeholder
    Component* component = new Component();
    component->SetEntityID(entityID);
    component->SetType(ComponentType::Transform);

    // TODO: Parse and apply transform properties
    // float posX = GetFloatField(jsonData, "positionX");
    // float posY = GetFloatField(jsonData, "positionY");
    // float posZ = GetFloatField(jsonData, "positionZ");
    // component->SetPosition(posX, posY, posZ);

    std::cout << "[ComponentFactory]   TransformComponent created (placeholder)" << std::endl;

    return component;
}

Component* ComponentFactory::CreateMeshRenderer(const std::string& jsonData, uint32_t entityID) {
    std::cout << "[ComponentFactory]   Creating MeshRenderer..." << std::endl;

    Component* component = new Component();
    component->SetEntityID(entityID);
    component->SetType(ComponentType::MeshRenderer);

    // TODO: Parse mesh properties
    // std::string meshPath = GetStringField(jsonData, "meshPath");
    // std::string materialPath = GetStringField(jsonData, "materialPath");

    std::cout << "[ComponentFactory]   MeshRenderer created (placeholder)" << std::endl;

    return component;
}

Component* ComponentFactory::CreateRigidBody(const std::string& jsonData, uint32_t entityID) {
    std::cout << "[ComponentFactory]   Creating RigidBody..." << std::endl;

    Component* component = new Component();
    component->SetEntityID(entityID);
    component->SetType(ComponentType::RigidBody);

    // TODO: Parse physics properties
    // float mass = GetFloatField(jsonData, "mass");
    // bool useGravity = GetBoolField(jsonData, "useGravity");

    std::cout << "[ComponentFactory]   RigidBody created (placeholder)" << std::endl;

    return component;
}

Component* ComponentFactory::CreateScript(const std::string& jsonData, uint32_t entityID) {
    std::cout << "[ComponentFactory]   Creating Script..." << std::endl;

    Component* component = new Component();
    component->SetEntityID(entityID);
    component->SetType(ComponentType::Script);

    // TODO: Parse script properties
    // std::string scriptPath = GetStringField(jsonData, "scriptPath");
    // std::string className = GetStringField(jsonData, "className");

    std::cout << "[ComponentFactory]   Script created (placeholder)" << std::endl;

    return component;
}

Component* ComponentFactory::CreateCameraComponent(const std::string& jsonData, uint32_t entityID) {
    std::cout << "[ComponentFactory]   Creating Camera..." << std::endl;

    Component* component = new Component();
    component->SetEntityID(entityID);
    component->SetType(ComponentType::Camera);

    // TODO: Parse camera properties
    // float fov = GetFloatField(jsonData, "fov");
    // float nearPlane = GetFloatField(jsonData, "nearPlane");
    // float farPlane = GetFloatField(jsonData, "farPlane");

    std::cout << "[ComponentFactory]   Camera created (placeholder)" << std::endl;

    return component;
}

Component* ComponentFactory::CreateLightComponent(const std::string& jsonData, uint32_t entityID) {
    std::cout << "[ComponentFactory]   Creating Light..." << std::endl;

    Component* component = new Component();
    component->SetEntityID(entityID);
    component->SetType(ComponentType::Light);

    // TODO: Parse light properties
    // std::string lightType = GetStringField(jsonData, "lightType");
    // float intensity = GetFloatField(jsonData, "intensity");

    std::cout << "[ComponentFactory]   Light created (placeholder)" << std::endl;

    return component;
}

//============================================================================
// JSON PARSING HELPERS
//============================================================================

std::string ComponentFactory::GetStringField(const std::string& jsonData, const std::string& fieldName) {
    // Simplified JSON parsing (replace with proper JSON library)
    std::string searchKey = "\"" + fieldName + "\":";
    size_t pos = jsonData.find(searchKey);

    if (pos == std::string::npos) {
        return "";
    }

    // Find opening quote
    size_t valueStart = jsonData.find("\"", pos + searchKey.length());
    if (valueStart == std::string::npos) {
        return "";
    }

    // Find closing quote
    size_t valueEnd = jsonData.find("\"", valueStart + 1);
    if (valueEnd == std::string::npos) {
        return "";
    }

    return jsonData.substr(valueStart + 1, valueEnd - valueStart - 1);
}

float ComponentFactory::GetFloatField(const std::string& jsonData, const std::string& fieldName) {
    std::string searchKey = "\"" + fieldName + "\":";
    size_t pos = jsonData.find(searchKey);

    if (pos == std::string::npos) {
        return 0.0f;
    }

    // Skip whitespace
    size_t valueStart = pos + searchKey.length();
    while (valueStart < jsonData.length() &&
           (jsonData[valueStart] == ' ' || jsonData[valueStart] == '\t')) {
        valueStart++;
    }

    // Extract number
    std::string valueStr;
    size_t i = valueStart;
    while (i < jsonData.length() &&
           (std::isdigit(jsonData[i]) || jsonData[i] == '.' || jsonData[i] == '-')) {
        valueStr += jsonData[i];
        i++;
    }

    if (valueStr.empty()) {
        return 0.0f;
    }

    return std::stof(valueStr);
}

int ComponentFactory::GetIntField(const std::string& jsonData, const std::string& fieldName) {
    return static_cast<int>(GetFloatField(jsonData, fieldName));
}

bool ComponentFactory::GetBoolField(const std::string& jsonData, const std::string& fieldName) {
    std::string value = GetStringField(jsonData, fieldName);
    return (value == "true" || value == "1");
}

//============================================================================
// ECS REGISTRATION
//============================================================================

void ComponentFactory::RegisterComponentWithECS(Component* component, uint32_t entityID) {
    // TODO: Integrate with ECS Coordinator
    // auto& ecs = ECSCoordinator::Get();
    // ecs.RegisterComponent(entityID, component);

    std::cout << "[ComponentFactory]   Registered component with ECS (placeholder)" << std::endl;
}

//============================================================================
// END OF FILE
//===============================================================