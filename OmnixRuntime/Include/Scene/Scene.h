//============================================================================
// Scene.h - Production Header
//
// Container and updater for the game world
// Holds root objects, camera, EntityID pool, and manages update cycle
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "Scene/Vector3.h"
#include "Scene/Quaternion.h"
#include "../ECS/ECSconfig.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Forward declarations
class SceneObject;
class IDPool;
class Coordinator;

/**
 * @brief Scene - Container and updater for the game world
 *
 * Responsibilities:
 * - Hold root objects and object hierarchy
 * - Manage EntityID assignment via pool
 * - Update transform hierarchy (local → world)
 * - Coordinate with ECS for system updates
 * - Handle scene lifecycle (Initialize, Update, Cleanup)
 *
 * Update Order:
 * 1. Update transform hierarchy
 * 2. Update SceneObjects
 * 3. ECS systems (Physics, Rendering, Animation, Input, Audio)
 * 4. Process destructions/late-additions
 */
class Scene {
public:
    //========================================================================
    // CONSTRUCTION / DESTRUCTION
    //========================================================================

    /**
     * @brief Constructor
     * @param name Scene name
     */
    explicit Scene(const std::string& name);

    /**
     * @brief Destructor - cleanup all objects
     */
    ~Scene();

    // Disallow copying
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    //========================================================================
    // CORE SCENE ALGORITHMS
    //========================================================================

    /**
     * @brief AddObject - Add SceneObject to scene
     *
     * Algorithm (from specification):
     * 1. Assign EntityID from pool if needed
     * 2. Insert into object map
     * 3. If no parent, add to root list
     * 4. Send EntityID + components to ECS Coordinator
     *
     * @param obj SceneObject to add
     */
    void AddSceneObject(std::shared_ptr<SceneObject> obj);

    /**
     * @brief Rebuilds the root objects list from the flat list of objects.
     */
    void RebuildRootObjects();

    /**
     * @brief Update - Main scene update loop
     *
     * Algorithm (from specification):
     * 1. Update transform hierarchy (local → world)
     * 2. Update each root SceneObject (calls object.Update())
     * 3. After SceneObjects finish, ECS Coordinator runs systems:
     *    - Physics
     *    - Rendering
     *    - Animation
     *    - Input
     *    - Audio
     * 4. Process destructions or late-additions
     *
     * @param deltaTime Time elapsed since last update
     */
    void Update(float deltaTime);

    //========================================================================
    // LIFECYCLE METHODS
    //========================================================================

    /**
     * @brief Initialize - Called after scene is loaded
     */
    void Initialize();

    /**
     * @brief Cleanup - Called before unloading scene
     */
    void Cleanup();

    //========================================================================
    // OBJECT ACCESS
    //========================================================================

    /**
     * @brief Get all SceneObjects in scene
     * @return Const reference to vector of all objects
     */
    const std::vector<std::shared_ptr<SceneObject>>& GetAllSceneObjects() const;

    /**
     * @brief Get root objects (objects with no parent)
     * @return Const reference to vector of root objects
     */
    const std::vector<std::shared_ptr<SceneObject>>& GetRootObjects() const;

    /**
     * @brief Find SceneObject by name
     * @param name Object name
     * @return Shared pointer to object (nullptr if not found)
     */
    std::shared_ptr<SceneObject> FindObjectByName(const std::string& name) const;

    /**
     * @brief Find SceneObject by EntityID
     * @param entityID Entity identifier
     * @return Shared pointer to object (nullptr if not found)
     */
    std::shared_ptr<SceneObject> FindObjectByID(uint32_t entityID) const;

    /**
     * @brief Remove SceneObject from scene
     * @param obj Object to remove
     */
    void RemoveSceneObject(std::shared_ptr<SceneObject> obj);

    //========================================================================
    // SCENE METADATA
    //========================================================================

    /**
     * @brief Get scene name
     * @return Scene name string
     */
    const std::string& GetName() const;

    /**
     * @brief Set file path this scene was loaded from
     * @param path File path
     */
    void SetFilePath(const std::string& path);

    /**
     * @brief Get file path
     * @return File path string
     */
    const std::string& GetFilePath() const;

    /**
     * @brief Set default camera name
     * @param cameraName Name of camera object
     */
    void SetDefaultCamera(const std::string& cameraName);

    /**
     * @brief Get default camera name
     * @return Camera name string
     */
    const std::string& GetDefaultCamera() const;

    //========================================================================
    // ENVIRONMENT SETTINGS
    //========================================================================

    /**
     * @brief Set ambient light
     * @param color Light color (RGB)
     * @param intensity Light intensity
     */
    void SetAmbientLight(const Vector3& color, float intensity);

    /**
     * @brief Get ambient light color
     * @return Color vector
     */
    const Vector3& GetAmbientLightColor() const;

    /**
     * @brief Get ambient light intensity
     * @return Intensity value
     */
    float GetAmbientLightIntensity() const;

    /**
     * @brief Set gravity
     * @param gravity Gravity vector (typically negative Y)
     */
    void SetGravity(const Vector3& gravity);

    /**
     * @brief Get gravity
     * @return Gravity vector
     */
    const Vector3& GetGravity() const;

    Scene* Clone(Coordinator& srcCoordinator, Coordinator& destCoordinator, std::unordered_map<Entity, Entity>& outEntityMap) const;
    bool CompareScene(const Scene& other, const std::unordered_map<Entity, Entity>& entityMap, Coordinator& coordinator) const;

    // Helper types for GPU-friendly light extraction
    struct LightTransformProxy {
        glm::vec3 position;
        glm::vec3 forward;
        glm::vec3 Forward() const { return forward; }
    };
    struct PointLightProxy {
        glm::vec3 color;
        float radius;
        float intensity;
    };
    struct SpotLightProxy {
        glm::vec3 color;
        float range;
        float intensity;
        float innerAngleDegrees;
        float outerAngleDegrees;
    };

    std::vector<uint32_t> GetPointLightEntities() const;
    std::vector<uint32_t> GetSpotLightEntities() const;
    LightTransformProxy GetTransform(uint32_t entityID) const;
    PointLightProxy GetPointLight(uint32_t entityID) const;
    SpotLightProxy GetSpotLight(uint32_t entityID) const;

private:
    //========================================================================
    // INTERNAL UPDATE HELPERS
    //========================================================================

    /**
     * @brief Update transform hierarchy (local → world transforms)
     * Called before object updates to ensure transforms are current
     */
    void UpdateTransformHierarchy();

    /**
     * @brief Update all root objects recursively
     * @param deltaTime Time delta
     */
    void UpdateRootObjects(float deltaTime);

    /**
     * @brief Trigger ECS Coordinator to run all systems
     * @param deltaTime Time delta
     */
    void RunECSSystems(float deltaTime);

    /**
     * @brief Process pending object destructions
     */
    void ProcessDestructions();

    /**
     * @brief Process pending object additions
     */
    void ProcessLateAdditions();

    /**
     * @brief Update lookup maps after adding/removing objects
     */
    void UpdateLookupMaps();

    /**
     * @brief Send object's components to ECS Coordinator
     * @param obj Object whose components to register
     */
    void RegisterObjectWithECS(std::shared_ptr<SceneObject> obj);

    //========================================================================
    // MEMBER VARIABLES
    //========================================================================

    // Scene metadata
    std::string name_;
    std::string filePath_;
    std::string defaultCamera_;

    // Object storage
    std::vector<std::shared_ptr<SceneObject>> allObjects_;       // All objects in scene
    std::vector<std::shared_ptr<SceneObject>> rootObjects_;      // Root objects (no parent)

    // Fast lookup maps
    std::unordered_map<std::string, std::shared_ptr<SceneObject>> nameMap_;  // Name → Object
    std::unordered_map<uint32_t, std::shared_ptr<SceneObject>> idMap_;       // EntityID → Object

    // Pending operations
    std::vector<std::shared_ptr<SceneObject>> pendingAdditions_;   // Objects to add next frame
    std::vector<std::shared_ptr<SceneObject>> pendingDeletions_;   // Objects to delete next frame

    // Environment settings
    Vector3 ambientLightColor_;
    float ambientLightIntensity_;
    Vector3 gravity_;

    // EntityID pool reference
    // Note: IDPool is typically a singleton, not owned by Scene
    // IDPool* idPo ol_;  // If you want scene-specific ID pools
};


//============================================================================
// END OF FILE
//===============================================================