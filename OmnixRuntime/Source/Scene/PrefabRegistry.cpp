//============================================================================
// PrefabRegistry.cpp - Implementation
//
// Prefab registry system with caching
// Implements Get algorithm
//
// Created: November 25, 2025
//============================================================================

#include "Scene/PrefabRegistry.h"
#include "Scene/Prefab.h"
#include "Scene/SceneObject.h"

#include <iostream>

//============================================================================
// CONSTRUCTION
//============================================================================

PrefabRegistry::PrefabRegistry()
    : prefabDirectory_("prefabs/")
{
    std::cout << "[PrefabRegistry] Initialized with directory: "
              << prefabDirectory_ << std::endl;
}

PrefabRegistry::~PrefabRegistry() = default;

//============================================================================
// CORE ALGORITHM: Get
//============================================================================

/**
 * @brief Get - Retrieve or load prefab
 *
 * Algorithm (from specification):
 * 1. Check loaded prefab map
 * 2. If not loaded:
 *    - Load file → Prefab
 *    - Cache it
 * 3. Return prefab
 */
Prefab* PrefabRegistry::Get(const std::string& prefabName) {
    // STEP 1: Check loaded prefab map
    auto it = prefabs_.find(prefabName);

    if (it != prefabs_.end()) {
        // Already loaded - return cached version
        std::cout << "[PrefabRegistry] Returning cached prefab: "
                  << prefabName << std::endl;
        return it->second.get();
    }

    // STEP 2: If not loaded - Load file → Prefab, Cache it
    std::cout << "[PrefabRegistry] Loading prefab: " << prefabName << std::endl;

    std::unique_ptr<Prefab> prefab = LoadPrefabFromFile(prefabName);

    if (!prefab) {
        std::cerr << "[PrefabRegistry] ERROR: Failed to load prefab: "
                  << prefabName << std::endl;
        return nullptr;
    }

    // Cache the loaded prefab
    Prefab* prefabPtr = prefab.get();
    prefabs_[prefabName] = std::move(prefab);

    std::cout << "[PrefabRegistry] Cached prefab: " << prefabName << std::endl;

    // STEP 3: Return prefab
    return prefabPtr;
}

//============================================================================
// PREFAB MANAGEMENT
//============================================================================

void PrefabRegistry::Register(const std::string& prefabName, std::unique_ptr<Prefab> prefab) {
    if (!prefab) {
        std::cerr << "[PrefabRegistry] ERROR: Cannot register null prefab: "
                  << prefabName << std::endl;
        return;
    }

    std::cout << "[PrefabRegistry] Registering prefab: " << prefabName << std::endl;

    prefabs_[prefabName] = std::move(prefab);
}

void PrefabRegistry::Unload(const std::string& prefabName) {
    auto it = prefabs_.find(prefabName);

    if (it != prefabs_.end()) {
        std::cout << "[PrefabRegistry] Unloading prefab: " << prefabName << std::endl;
        prefabs_.erase(it);
    } else {
        std::cout << "[PrefabRegistry] WARNING: Prefab not loaded: "
                  << prefabName << std::endl;
    }
}

void PrefabRegistry::UnloadAll() {
    std::cout << "[PrefabRegistry] Unloading all " << prefabs_.size()
              << " prefabs..." << std::endl;
    prefabs_.clear();
}

bool PrefabRegistry::IsLoaded(const std::string& prefabName) const {
    return prefabs_.find(prefabName) != prefabs_.end();
}

size_t PrefabRegistry::GetLoadedCount() const {
    return prefabs_.size();
}

//============================================================================
// PATH CONFIGURATION
//============================================================================

void PrefabRegistry::SetPrefabDirectory(const std::string& path) {
    prefabDirectory_ = path;

    // Ensure trailing slash
    if (!prefabDirectory_.empty() && prefabDirectory_.back() != '/' && prefabDirectory_.back() != '\\') {
        prefabDirectory_ += '/';
    }

    std::cout << "[PrefabRegistry] Set prefab directory: " << prefabDirectory_ << std::endl;
}

const std::string& PrefabRegistry::GetPrefabDirectory() const {
    return prefabDirectory_;
}

//============================================================================
// INTERNAL LOADING HELPERS
//============================================================================

/**
 * @brief LoadPrefabFromFile - Load prefab from disk
 *
 * This is where you'd integrate with SceneLoader to load prefab JSON files.
 */
std::unique_ptr<Prefab> PrefabRegistry::LoadPrefabFromFile(const std::string& prefabName) {
    // Construct full file path
    std::string filePath = GetPrefabFilePath(prefabName);

    std::cout << "[PrefabRegistry] Loading from file: " << filePath << std::endl;

    // Create prefab object
    auto prefab = std::make_unique<Prefab>(filePath);

    // TODO: Integrate with SceneLoader to load prefab template from JSON
    // For now, create a placeholder template

    // Placeholder implementation:
    // auto templateObj = SceneLoader::LoadPrefabTemplate(filePath);
    // prefab->SetTemplateObject(templateObj);

    // Temporary placeholder:
    std::cout << "[PrefabRegistry] WARNING: Prefab loading not fully implemented. "
              << "Create template manually or integrate SceneLoader." << std::endl;

    return prefab;
}

/**
 * @brief GetPrefabFilePath - Construct full path to prefab file
 */
std::string PrefabRegistry::GetPrefabFilePath(const std::string& prefabName) const {
    // If prefabName already has extension, use as-is
    if (prefabName.find(".json") != std::string::npos) {
        return prefabDirectory_ + prefabName;
    }

    // Otherwise, add .json extension
    return prefabDirectory_ + prefabName + ".json";
}

//============================================================================
// END OF FILE
//===============================================================