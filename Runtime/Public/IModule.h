#pragma once

#include <string>
#include <vector>

namespace eng::runtime {

    struct RuntimeContext;

    /**
     * @brief IModule - Interface for modular runtime engine subsystems.
     * 
     * Implementations of this interface represent modules that can be dynamically
     * loaded, updated, and shutdown by the EngineRuntime module management system.
     */
    class IModule {
    public:
        virtual ~IModule() = default;

        /**
         * @brief Initialize the module.
         * @param context Reference to the global RuntimeContext.
         * @return True if initialization succeeded, false otherwise.
         */
        virtual bool Init(RuntimeContext& context) = 0;

        /**
         * @brief Shutdown and clean up any allocated resources.
         */
        virtual void Shutdown() = 0;

        /**
         * @brief Tick/Update the module. Called once per frame.
         * @param deltaTime Elapsed time since the last frame in seconds.
         */
        virtual void Tick(float deltaTime) = 0;

        /**
         * @brief Get the unique name of this module.
         * @return The string name of the module.
         */
        [[nodiscard]] virtual std::string GetName() const = 0;

        /**
         * @brief Get the list of modules that this module depends on.
         * @return Vector of dependency module names.
         */
        [[nodiscard]] virtual std::vector<std::string> GetDependencies() const = 0;
    };

} // namespace eng::runtime
