// ComponentFactory.h - Component Factory System
//
// Handles component creation from JSON data
// Implements CreateFromJSON algorithm
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <string>
#include <memory>
#include <cstdint>

// Forward declarations
class Component;

/**
 * @brief ComponentFactory - Creates components from JSON data
 *
 * Factory pattern for creating components based on type information.
 * Handles deserialization and registration with ECS.
 */
class ComponentFactory {
public:
    /**
     * @brief Component type enumeration
     */
    enum class ComponentType {
        Unknown,
        Transform,
        MeshRenderer,
        RigidBody,
        Script,
        Camera,
        Light,
        AudioSource,
        Collider,
        Animator
    };

    //========================================================================
    // CORE ALGORITHM: CreateFromJSON
    //========================================================================

    /**
     * @brief CreateFromJSON - Create component from JSON data
     *
     * Algorithm (from specification):
     * 1. Read component type field
     * 2. Switch(type):
     *    - TransformComponent
     *    - MeshRenderer
     *    - RigidBody
     *    - Script
     *    - etc.
     * 3. Allocate new component
     * 4. Apply JSON properties
     * 5. Register component into ECS storage under entityID
     * 6. Return created component
     *
     * @param jsonData JSON string containing component data
     * @param entityID Entity ID to attach component to
     * @return Pointer to created component (nullptr on failure)
     */
    static Component* CreateFromJSON(const std::string& jsonData, uint32_t entityID);

    //========================================================================
    // TYPE PARSING
    //========================================================================

    /**
     * @brief Parse component type from JSON
     * @param jsonData JSON string
     * @return Component type enum
     */
    static ComponentType ParseComponentType(const std::string& jsonData);

    /**
     * @brief Convert string to component type
     * @param typeName Type name string
     * @return Component type enum
     */
    static ComponentType StringToComponentType(const std::string& typeName);

    /**
     * @brief Convert component type to string
     * @param type Component type enum
     * @return Type name string
     */
    static std::string ComponentTypeToString(ComponentType type);

private:
    //========================================================================
    // COMPONENT CREATION HELPERS
    //========================================================================

    /**
     * @brief Create TransformComponent from JSON
     * @param jsonData JSON data
     * @param entityID Entity ID
     * @return Created component
     */
    static Component* CreateTransformComponent(const std::string& jsonData, uint32_t entityID);

    /**
     * @brief Create MeshRenderer from JSON
     * @param jsonData JSON data
     * @param entityID Entity ID
     * @return Created component
     */
    static Component* CreateMeshRenderer(const std::string& jsonData, uint32_t entityID);

    /**
     * @brief Create RigidBody from JSON
     * @param jsonData JSON data
     * @param entityID Entity ID
     * @return Created component
     */
    static Component* CreateRigidBody(const std::string& jsonData, uint32_t entityID);

    /**
     * @brief Create Script component from JSON
     * @param jsonData JSON data
     * @param entityID Entity ID
     * @return Created component
     */
    static Component* CreateScript(const std::string& jsonData, uint32_t entityID);

    /**
     * @brief Create Camera component from JSON
     * @param jsonData JSON data
     * @param entityID Entity ID
     * @return Created component
     */
    static Component* CreateCameraComponent(const std::string& jsonData, uint32_t entityID);

    /**
     * @brief Create Light component from JSON
     * @param jsonData JSON data
     * @param entityID Entity ID
     * @return Created component
     */
    static Component* CreateLightComponent(const std::string& jsonData, uint32_t entityID);

    //========================================================================
    // JSON PARSING HELPERS
    //========================================================================

    /**
     * @brief Extract string field from JSON
     * @param jsonData JSON data
     * @param fieldName Field name
     * @return Field value (empty if not found)
     */
    static std::string GetStringField(const std::string& jsonData, const std::string& fieldName);

    /**
     * @brief Extract float field from JSON
     * @param jsonData JSON data
     * @param fieldName Field name
     * @return Field value (0.0f if not found)
     */
    static float GetFloatField(const std::string& jsonData, const std::string& fieldName);

    /**
     * @brief Extract int field from JSON
     * @param jsonData JSON data
     * @param fieldName Field name
     * @return Field value (0 if not found)
     */
    static int GetIntField(const std::string& jsonData, const std::string& fieldName);

    /**
     * @brief Extract bool field from JSON
     * @param jsonData JSON data
     * @param fieldName Field name
     * @return Field value (false if not found)
     */
    static bool GetBoolField(const std::string& jsonData, const std::string& fieldName);

    //========================================================================
    // ECS REGISTRATION
    //========================================================================

    /**
     * @brief Register component with ECS
     * @param component Component to register
     * @param entityID Entity ID
     */
    static void RegisterComponentWithECS(Component* component, uint32_t entityID);
};

//============================================================================
// Component Base Class (Placeholder)
//============================================================================

/**
 * @brief Component - Base class for all components
 */
class Component {
public:
    virtual ~Component() = default;

    uint32_t GetEntityID() const { return entityID_; }
    void SetEntityID(uint32_t id) { entityID_ = id; }

    ComponentFactory::ComponentType GetType() const { return type_; }
    void SetType(ComponentFactory::ComponentType type) { type_ = type; }

protected:
    uint32_t entityID_ = 0;
    ComponentFactory::ComponentType type_ = ComponentFactory::ComponentType::Unknown;
};
static ComponentFactory& GetInstance() {
    static ComponentFactory instance;
    return instance;
}


//============================================================================
// END OF FILE
//================================================================