//============================================================================
// Prefab.h - Prefab System
//
// Handles prefab instantiation and management
// Implements Instantiate algorithm
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <string>
#include <memory>
#include <vector>

// Forward declarations
class SceneObject;

/**
 * @brief Prefab - Reusable template for scene objects
 *
 * A prefab stores a template object hierarchy that can be instantiated
 * multiple times in a scene. Each instantiation creates a deep copy
 * with new EntityIDs.
 */
class Prefab {
public:
    //========================================================================
    // CONSTRUCTION
    //========================================================================

    /**
     * @brief Constructor
     * @param prefabPath Path to prefab file
     */
    explicit Prefab(const std::string& prefabPath);

    /**
     * @brief Destructor
     */
    ~Prefab();

    //========================================================================
    // CORE ALGORITHM: Instantiate
    //========================================================================

    /**
     * @brief Instantiate - Create instance of prefab
     *
     * Algorithm (from specification):
     * 1. Deep-copy base object tree
     * 2. Generate new EntityIDs via Scene's ID pool
     * 3. For each object copy:
     *    - Copy components
     *    - Copy transform
     *    - Rebuild children
     * 4. Return instantiated root object
     *
     * @return Shared pointer to instantiated root object
     */
    std::shared_ptr<SceneObject> Instantiate();

    //========================================================================
    // PREFAB PROPERTIES
    //========================================================================

    /**
     * @brief Get prefab path
     * @return Path to prefab file
     */
    const std::string& GetPath() const;

    /**
     * @brief Set template object
     * @param templateObj Root object to use as template
     */
    void SetTemplateObject(std::shared_ptr<SceneObject> templateObj);

    /**
     * @brief Get template object
     * @return Template root object
     */
    std::shared_ptr<SceneObject> GetTemplateObject() const;

    /**
     * @brief Get prefab name
     * @return Prefab name (extracted from path)
     */
    std::string GetName() const;

private:
    //========================================================================
    // INTERNAL INSTANTIATION HELPERS
    //========================================================================

    /**
     * @brief Deep copy a SceneObject and its entire hierarchy
     * @param source Source object to copy
     * @return Deep copy of object
     */
    std::shared_ptr<SceneObject> DeepCopyObject(SceneObject* source);

    /**
     * @brief Copy object's components
     * @param source Source object
     * @param destination Destination object
     */
    void CopyComponents(SceneObject* source, SceneObject* destination);

    /**
     * @brief Copy object's transform
     * @param source Source object
     * @param destination Destination object
     */
    void CopyTransform(SceneObject* source, SceneObject* destination);

    /**
     * @brief Rebuild children hierarchy
     * @param source Source parent object
     * @param destination Destination parent object
     */
    void RebuildChildren(SceneObject* source, SceneObject* destination);

    /**
     * @brief Generate new EntityID for object
     * @param object Object to assign new ID
     */
    void GenerateNewEntityID(SceneObject* object);

    //========================================================================
    // MEMBER VARIABLES
    //========================================================================

    std::string prefabPath_;                        // Path to prefab file
    std::shared_ptr<SceneObject> templateObject_;   // Template root object
};

namespace eng::scene {
    using Prefab = ::Prefab;
}

//============================================================================
// END OF FILE
//===============================================================