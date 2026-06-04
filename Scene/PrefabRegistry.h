//============================================================================
// PrefabRegistry.h - Prefab Registry System
//
// Manages prefab loading and caching
// Implements Get algorithm
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <string>
#include <memory>
#include <unordered_map>

// Forward declarations
class Prefab;
class SceneObject;

/**
 * @brief PrefabRegistry - Singleton prefab manager
 *
 * Manages loading, caching, and access to prefabs.
 * Ensures each prefab is loaded only once and reused.
 */
class PrefabRegistry {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to PrefabRegistry singleton
     */
    static PrefabRegistry& Get() {
        static PrefabRegistry instance;
        return instance;
    }

    //========================================================================
    // CORE ALGORITHM: Get
    //========================================================================

    /**
     * @brief Get - Retrieve or load prefab
     *
     * Algorithm (from specification):
     * 1. Check loaded prefab map
     * 2. If not loaded:
     *    - Load file → Prefab
     *    - Cache it
     * 3. Return prefab
     *
     * @param prefabName Name of prefab to retrieve
     * @return Pointer to prefab (nullptr if not found)
     */
    Prefab* Get(const std::string& prefabName);

    //========================================================================
    // PREFAB MANAGEMENT
    //========================================================================

    /**
     * @brief Register prefab manually
     * @param prefabName Name to register under
     * @param prefab Prefab to register
     */
    void Register(const std::string& prefabName, std::unique_ptr<Prefab> prefab);

    /**
     * @brief Unload prefab from cache
     * @param prefabName Name of prefab to unload
     */
    void Unload(const std::string& prefabName);

    /**
     * @brief Unload all prefabs
     */
    void UnloadAll();

    /**
     * @brief Check if prefab is loaded
     * @param prefabName Name of prefab
     * @return True if loaded in cache
     */
    bool IsLoaded(const std::string& prefabName) const;

    /**
     * @brief Get count of loaded prefabs
     * @return Number of prefabs in cache
     */
    size_t GetLoadedCount() const;

    //========================================================================
    // PATH CONFIGURATION
    //========================================================================

    /**
     * @brief Set prefab directory path
     * @param path Directory containing prefab files
     */
    void SetPrefabDirectory(const std::string& path);

    /**
     * @brief Get prefab directory path
     * @return Prefab directory
     */
    const std::string& GetPrefabDirectory() const;

private:
    // Private constructor for singleton
    PrefabRegistry();

    // Prevent copying
    PrefabRegistry(const PrefabRegistry&) = delete;
    PrefabRegistry& operator=(const PrefabRegistry&) = delete;

    //========================================================================
    // INTERNAL LOADING HELPERS
    //========================================================================

    /**
     * @brief Load prefab from file
     * @param prefabName Name of prefab
     * @return Unique pointer to loaded prefab
     */
    std::unique_ptr<Prefab> LoadPrefabFromFile(const std::string& prefabName);

    /**
     * @brief Construct full path to prefab file
     * @param prefabName Name of prefab
     * @return Full file path
     */
    std::string GetPrefabFilePath(const std::string& prefabName) const;

    //========================================================================
    // MEMBER VARIABLES
    //========================================================================

    std::unordered_map<std::string, std::unique_ptr<Prefab>> prefabs_;  // Cached prefabs
    std::string prefabDirectory_;  // Directory containing prefab files
};

//============================================================================
// END OF FILE
//===============================================================