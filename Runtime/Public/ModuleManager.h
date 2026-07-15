#pragma once

#include "Runtime/Public/IModule.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

namespace eng::runtime {

    struct RuntimeContext;

    /**
     * @class ModuleManager
     * @brief Manages module lifetimes, dependency resolution, initialization, updates, and shutdown.
     */
    class ModuleManager {
    public:
        ModuleManager() = default;
        ~ModuleManager() { ShutdownModules(); }

        /**
         * @brief Register a module. Must be called before InitializeModules.
         */
        void RegisterModule(std::shared_ptr<IModule> module);

        /**
         * @brief Resolves dependencies topologically and initializes all registered modules.
         */
        bool InitializeModules(RuntimeContext& context);

        /**
         * @brief Shuts down all modules in reverse topological order.
         */
        void ShutdownModules();

        /**
         * @brief Updates (Ticks) all initialized modules in dependency-safe order.
         */
        void TickModules(float deltaTime);

        /**
         * @brief Retrieves a module by name.
         */
        [[nodiscard]] std::shared_ptr<IModule> GetModule(const std::string& name) const;

        /**
         * @brief Gets all modules sorted in their initialization order.
         */
        [[nodiscard]] const std::vector<std::shared_ptr<IModule>>& GetLoadedModulesSorted() const { return m_SortedModules; }

    private:
        bool BuildTopologicalOrder();

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, std::shared_ptr<IModule>> m_Modules;
        std::vector<std::shared_ptr<IModule>> m_SortedModules;
        bool m_Initialized = false;
    };

} // namespace eng::runtime
