#include "Runtime/Public/ModuleManager.h"
#include "Core/Logging/Logger.h"
#include <queue>
#include <set>
#include <algorithm>

namespace eng::runtime {

    void ModuleManager::RegisterModule(std::shared_ptr<IModule> module) {
        if (!module) return;
        std::lock_guard<std::mutex> lock(m_Mutex);
        std::string name = module->GetName();
        m_Modules[name] = module;
        CORE_LOG_INFO("[ModuleManager] Registered module: %s", name.c_str());
    }

    bool ModuleManager::InitializeModules(RuntimeContext& context) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Initialized) return true;

        if (!BuildTopologicalOrder()) {
            CORE_LOG_ERROR("[ModuleManager] Failed to build dependency order. Initialization aborted.");
            return false;
        }

        CORE_LOG_INFO("[ModuleManager] Initializing modules in dependency-sorted order:");
        for (auto& module : m_SortedModules) {
            CORE_LOG_INFO("  -> %s", module->GetName().c_str());
            if (!module->Init(context)) {
                CORE_LOG_ERROR("[ModuleManager] Module %s failed to initialize!", module->GetName().c_str());
                // Rollback already initialized modules in reverse order
                for (auto it = m_SortedModules.rbegin(); it != m_SortedModules.rend(); ++it) {
                    if (*it == module) {
                        continue;
                    }
                    (*it)->Shutdown();
                }
                return false;
            }
        }

        m_Initialized = true;
        return true;
    }

    void ModuleManager::ShutdownModules() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Initialized) return;

        CORE_LOG_INFO("[ModuleManager] Shutting down modules in reverse dependency order:");
        for (auto it = m_SortedModules.rbegin(); it != m_SortedModules.rend(); ++it) {
            CORE_LOG_INFO("  <- %s", (*it)->GetName().c_str());
            (*it)->Shutdown();
        }

        m_SortedModules.clear();
        m_Initialized = false;
    }

    void ModuleManager::TickModules(float deltaTime) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Initialized) return;

        for (auto& module : m_SortedModules) {
            module->Tick(deltaTime);
        }
    }

    std::shared_ptr<IModule> ModuleManager::GetModule(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Modules.find(name);
        if (it != m_Modules.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool ModuleManager::BuildTopologicalOrder() {
        m_SortedModules.clear();

        // 1. Map name -> in-degree and build adjacency list
        std::unordered_map<std::string, int> inDegree;
        std::unordered_map<std::string, std::vector<std::string>> adj;

        // Initialize in-degree for all registered modules to 0
        for (const auto& [name, module] : m_Modules) {
            inDegree[name] = 0;
        }

        // Build edges
        for (const auto& [name, module] : m_Modules) {
            auto deps = module->GetDependencies();
            for (const auto& dep : deps) {
                // If a dependency is listed but not registered, fail
                if (m_Modules.find(dep) == m_Modules.end()) {
                    CORE_LOG_ERROR("[ModuleManager] Module '%s' depends on '%s', but '%s' is not registered!",
                                   name.c_str(), dep.c_str(), dep.c_str());
                    return false;
                }
                // dep -> name edge (dep must be loaded before name)
                adj[dep].push_back(name);
                inDegree[name]++;
            }
        }

        // 2. Queue nodes with 0 in-degree (dependency-free)
        std::queue<std::string> q;
        std::set<std::string> zeroDepSet;
        for (const auto& [name, degree] : inDegree) {
            if (degree == 0) {
                zeroDepSet.insert(name);
            }
        }
        for (const auto& name : zeroDepSet) {
            q.push(name);
        }

        // 3. Kahn's Algorithm
        while (!q.empty()) {
            std::string u = q.front();
            q.pop();

            m_SortedModules.push_back(m_Modules[u]);

            // Decrement in-degree of dependent modules
            auto& children = adj[u];
            std::sort(children.begin(), children.end());

            for (const auto& v : children) {
                inDegree[v]--;
                if (inDegree[v] == 0) {
                    q.push(v);
                }
            }
        }

        // Check for cycles
        if (m_SortedModules.size() != m_Modules.size()) {
            CORE_LOG_ERROR("[ModuleManager] Circular dependency detected in module configuration!");
            return false;
        }

        return true;
    }

} // namespace eng::runtime
