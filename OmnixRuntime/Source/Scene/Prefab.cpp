//============================================================================
// Prefab.cpp - Implementation
//
// Prefab instantiation system
// Implements Instantiate algorithm
//
// Created: November 25, 2025
//============================================================================

#include "Scene/Prefab.h"
#include "Scene/SceneObject.h"
#include "Scene/IDPool.h"

#include <iostream>

//============================================================================
// CONSTRUCTION
//============================================================================

Prefab::Prefab(const std::string& prefabPath)
    : prefabPath_(prefabPath)
    , templateObject_(nullptr)
{
    std::cout << "[Prefab] Created prefab: " << prefabPath_ << std::endl;
}

Prefab::~Prefab() {
    std::cout << "[Prefab] Destroying prefab: " << prefabPath_ << std::endl;
}

//============================================================================
// CORE ALGORITHM: Instantiate
//============================================================================

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
 */
std::shared_ptr<SceneObject> Prefab::Instantiate() {
    if (!templateObject_) {
        std::cerr << "[Prefab] ERROR: No template object set for prefab: "
                  << prefabPath_ << std::endl;
        return nullptr;
    }

    std::cout << "[Prefab] Instantiating prefab: " << prefabPath_ << std::endl;

    // STEP 1: Deep-copy base object tree
    std::shared_ptr<SceneObject> instance = DeepCopyObject(templateObject_.get());

    if (!instance) {
        std::cerr << "[Prefab] ERROR: Failed to instantiate prefab: "
                  << prefabPath_ << std::endl;
        return nullptr;
    }

    std::cout << "[Prefab] Successfully instantiated: " << instance->GetName()
              << " (ID: " << instance->GetID() << ")" << std::endl;

    // STEP 4: Return instantiated root object
    return instance;
}

//============================================================================
// INTERNAL INSTANTIATION HELPERS
//============================================================================

/**
 * @brief DeepCopyObject - Recursively copy object and hierarchy
 *
 * This implements steps 2-3 of the algorithm:
 * 2. Generate new EntityIDs
 * 3. Copy components, transform, rebuild children
 */
std::shared_ptr<SceneObject> Prefab::DeepCopyObject(SceneObject* source) {
    if (!source) return nullptr;

    std::cout << "[Prefab]   Copying object: " << source->GetName() << std::endl;

    // Create new object with same name
    auto copy = std::make_shared<SceneObject>(source->GetName() + "_Instance");

    // STEP 2: Generate new EntityID via Scene's ID pool
    // (Already done in SceneObject constructor via IDPool::Get().RequestID())

    // STEP 3: For each object copy:

    // 3a. Copy components
    CopyComponents(source, copy.get());

    // 3b. Copy transform
    CopyTransform(source, copy.get());

    // 3c. Rebuild children
    RebuildChildren(source, copy.get());

    return copy;
}

/**
 * @brief CopyComponents - Copy all components from source to destination
 */
void Prefab::CopyComponents(SceneObject* source, SceneObject* destination) {
    // TODO: Implement component copying when component system is integrated
    // This will iterate through source's components and create copies

    // Placeholder implementation:
    // auto& ecs = ECSCoordinator::Get();
    // auto components = ecs.GetAllComponents(source->GetID());
    // for (auto& component : components) {
    //     auto componentCopy = component->Clone();
    //     ecs.AddComponent(destination->GetID(), componentCopy);
    // }

    std::cout << "[Prefab]     Copying components (placeholder)" << std::endl;
}

/**
 * @brief CopyTransform - Copy transform from source to destination
 */
void Prefab::CopyTransform(SceneObject* source, SceneObject* destination) {
    // Copy local transform (not world transform)
    destination->transform.SetPosition(source->transform.GetPosition());
    destination->transform.SetRotation(source->transform.GetRotation());
    destination->transform.SetScale(source->transform.GetScale());

    std::cout << "[Prefab]     Copied transform" << std::endl;
}

/**
 * @brief RebuildChildren - Recursively copy and rebuild child hierarchy
 */
void Prefab::RebuildChildren(SceneObject* source, SceneObject* destination) {
    const auto& sourceChildren = source->GetChildren();

    if (sourceChildren.empty()) {
        return;  // No children to rebuild
    }

    std::cout << "[Prefab]     Rebuilding " << sourceChildren.size()
              << " children..." << std::endl;

    // Recursively copy each child
    for (auto* sourceChild : sourceChildren) {
        if (sourceChild) {
            // Deep copy child (this recursively copies its children too)
            auto childCopy = DeepCopyObject(sourceChild);

            if (childCopy) {
                // Add to destination's children
                destination->AddChild(childCopy);

                std::cout << "[Prefab]       Added child: "
                          << childCopy->GetName() << std::endl;
            }
        }
    }
}

/**
 * @brief GenerateNewEntityID - Assign new EntityID to object
 */
void Prefab::GenerateNewEntityID(SceneObject* object) {
    // Request new ID from pool
    uint32_t newID = IDPool::Get().RequestID();

    // Set new ID on object
    object->SetID(newID);

    std::cout << "[Prefab]     Generated new EntityID: " << newID << std::endl;
}

//============================================================================
// PREFAB PROPERTIES
//============================================================================

const std::string& Prefab::GetPath() const {
    return prefabPath_;
}

void Prefab::SetTemplateObject(std::shared_ptr<SceneObject> templateObj) {
    templateObject_ = templateObj;
    std::cout << "[Prefab] Set template object: " << templateObj->GetName() << std::endl;
}

std::shared_ptr<SceneObject> Prefab::GetTemplateObject() const {
    return templateObject_;
}

std::string Prefab::GetName() const {
    // Extract name from path (e.g., "prefabs/Enemy.json" -> "Enemy")
    size_t lastSlash = prefabPath_.find_last_of("/\\");
    size_t lastDot = prefabPath_.find_last_of(".");

    if (lastSlash == std::string::npos) lastSlash = 0;
    else lastSlash++;

    if (lastDot == std::string::npos || lastDot < lastSlash) {
        lastDot = prefabPath_.length();
    }

    return prefabPath_.substr(lastSlash, lastDot - lastSlash);
}

//============================================================================
// END OF FILE
//===============================================================