#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include "Scene/Public/ISceneManager.h"

// Forward declarations
class Coordinator;
class Scene;
class SceneObject;

/**
 * @brief SceneManager - Singleton scene lifecycle manager
 *
 * Responsibilities:
 * - Scene switching and loading
 * - Transition state management
 * - ECS Coordinator integration
 * - Scene lifecycle (Initialize, Update, Cleanup)
 *
 * Usage:
 *   SceneManager::Instance().LoadScene("assets/scenes/Level1.json");
 *   SceneManager::Instance().Update(deltaTime);
 */

class SceneManager : public eng::runtime::ISceneManager {
public:
    //========================================================================
    // PUBLIC TYPES
    //========================================================================

    /**
     * @brief Transition state machine states
     */
    Coordinator& GetCoordinator();

    enum class TransitionState {
        Running,         // Scene is actively running
        Loading,         // Scene is being loaded
        ReadyToSwitch    // Scene loaded, ready to switch
    };

    //========================================================================
    // LIFECYCLE
    //========================================================================

    SceneManager(Coordinator* coordinator = nullptr);
    ~SceneManager() override;

    void SetCoordinator(Coordinator* coordinator);

    bool SaveActiveScene(const std::string& filePath);
    void CreateNewScene(const std::string& name);
    void SyncECSToScene();

    //========================================================================
    // CORE PUBLIC INTERFACE
    //========================================================================

    /**
     * @brief Main update loop - call every frame
     * @param dt Delta time since last frame
     *
     * Algorithm:
     * - If state == LOADING: Process scene loading
     * - If state == READY_TO_SWITCH: Switch to new scene
     * - If state == RUNNING: Update active scene
     */
    void Update(float dt) override;

    /**
     * @brief Request to load a new scene (async)
     * @param sceneName Path to scene file (e.g., "assets/scenes/Level1.json")
     *
     * Algorithm:
     * 1. Store sceneName as target
     * 2. Set state to LOADING
     * 3. Notify TransitionManager (if enabled)
     * 4. Begin loading process
     */
    void LoadScene(const std::string& sceneName) override;

    /**
     * @brief Finalize scene switch (internal, called by Update)
     *
     * Algorithm:
     * 1. Validate state == READY_TO_SWITCH
     * 2. Unload old scene (unregister entities from ECS)
     * 3. Activate new scene
     * 4. Register entities with ECS
     * 5. Initialize scene
     * 6. Set state to RUNNING
     */
    void SwitchScene() override;

    //========================================================================
    // SCENE ACCESS
    //========================================================================

    /**
     * @brief Get currently active scene
     * @return Pointer to active scene (nullptr if none)
     */
    Scene* GetActiveScene() const;
    void SetActiveScene(Scene* scene);

    /**
     * @brief Find SceneObject in active scene by name
     * @param name Object name to search for
     * @return Pointer to SceneObject (nullptr if not found)
     */
    SceneObject* GetSceneObjectByName(const std::string& name) const;

    /**
     * @brief Find SceneObject in active scene by entity ID
     * @param entityID Entity ID to search for
     * @return Pointer to SceneObject (nullptr if not found)
     */
    SceneObject* GetSceneObjectByID(uint32_t entityID) const;

    //========================================================================
    // STATE QUERIES
    //========================================================================

    /**
     * @brief Check if scene is currently loading
     * @return True if loading in progress
     */
    bool IsLoading() const;

    /**
     * @brief Get current transition state
     * @return Current TransitionState enum value
     */
    TransitionState GetTransitionState() const;

    //========================================================================
    // CONFIGURATION
    //========================================================================

    /**
     * @brief Enable/disable visual transitions (fade in/out)
     * @param enabled True to enable visual transitions
     */
    void SetTransitionsEnabled(bool enabled);

    //========================================================================
    // UTILITY METHODS
    //========================================================================

    /**
     * @brief Reload the currently active scene
     * Useful for quick iteration during development
     */
    void ReloadCurrentScene() override;

    /**
     * @brief Print debug information to console
     * Shows current state, active scene, loading status
     */
    void PrintDebugInfo() const;

private:
    Coordinator* m_Coordinator = nullptr;
    std::unique_ptr<Coordinator> m_OwnedCoordinator;
    bool m_OwnsCoordinator = false;
    void InitializeECS();

    // Delete copy and move constructors/operators
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

    //========================================================================
    // PRIVATE IMPLEMENTATION METHODS
    //========================================================================

    /**
     * @brief Process scene loading
     *
     * Algorithm:
     * 1. Validate state == LOADING
     * 2. Call SceneLoader to load scene file
     * 3. Store loaded scene in temporary buffer
     * 4. Set state to READY_TO_SWITCH when complete
     */
    void ProcessLoading();

    /**
     * @brief Unload and cleanup active scene
     * - Unregisters all entities from ECS Coordinator
     * - Destroys scene object hierarchy
     * - Frees scene memory
     */
    void UnloadActiveScene();

    /**
     * @brief Register all scene entities with ECS Coordinator
     * @param scene Scene whose entities need registration
     */
    void RegisterSceneEntities(Scene* scene);

    //========================================================================
    // MEMBER VARIABLES
    //========================================================================

    // Transition State Machine
    TransitionState state;

    // Scene Pointers
    Scene* activeScene;        // Currently running scene
    Scene* loadingScene;       // Scene being loaded (temp buffer)

    // Loading State
    std::string targetSceneName;  // Path to scene being loaded
    bool loadingComplete;         // Flag for async loading completion

    // Configuration
    bool transitionsEnabled;      // Enable visual fade transitions
};

//============================================================================
// INLINE HELPER FUNCTIONS (Optional)
//============================================================================
