#include "Runtime/PluginManager.h"
#include "Runtime/IModule.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    IModule* PluginManager::LoadPlugin(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_Mutex);

        auto it = m_Plugins.find(path);
        if (it != m_Plugins.end()) {
            CORE_LOG_WARN("[PluginManager] Plugin already loaded: %s", path.c_str());
            if (it->second.info.CreateModuleFn) {
                IModule* mod = it->second.info.CreateModuleFn();
                if (mod) {
                    it->second.activeModules.push_back(mod);
                }
                return mod;
            }
            return nullptr;
        }

        eng::platform::DynamicLibrary handle;
        auto loadRes = eng::platform::DynamicLibrary::Load(path, handle);
        if (loadRes != eng::core::ResultCode::Success) {
            CORE_LOG_ERROR("[PluginManager] Failed to load dynamic library: %s", path.c_str());
            return nullptr;
        }

        auto getInfo = reinterpret_cast<GetPluginInfoFn>(handle.GetSymbol("GetPluginInfo"));
        if (!getInfo) {
            CORE_LOG_ERROR("[PluginManager] Symbol 'GetPluginInfo' not found in plugin: %s", path.c_str());
            return nullptr;
        }

        PluginInfo info = getInfo();

        // ABI check (T1.1.4)
        if (info.abiVersion != ENGINE_ABI_VERSION) {
            CORE_LOG_ERROR("[PluginManager] Plugin ABI mismatch for %s. Expected: 0x%08X, Found: 0x%08X",
                           path.c_str(), ENGINE_ABI_VERSION, info.abiVersion);
            return nullptr;
        }

        CORE_LOG_INFO("[PluginManager] Successfully loaded plugin: %s (Version: %s)",
                       info.name, info.version);

        LoadedPlugin plugin{};
        plugin.path = path;
        plugin.handle = std::move(handle);
        plugin.info = info;

        IModule* mod = nullptr;
        if (info.CreateModuleFn) {
            mod = info.CreateModuleFn();
            if (mod) {
                plugin.activeModules.push_back(mod);
            }
        }

        m_Plugins[path] = std::move(plugin);
        return mod;
    }

    bool PluginManager::UnloadPlugin(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_Mutex);

        auto it = m_Plugins.find(path);
        if (it == m_Plugins.end()) {
            return false;
        }

        auto& plugin = it->second;

        // Hot-unload safety: refuse unload if plugin has live references (T1.1.5)
        if (!plugin.activeModules.empty()) {
            CORE_LOG_WARN("[PluginManager] Refusing to unload plugin '%s' because it has %zu live module references.",
                          plugin.info.name, plugin.activeModules.size());
            return false;
        }

        CORE_LOG_INFO("[PluginManager] Unloading plugin: %s", plugin.info.name);

        plugin.handle.Unload();

        m_Plugins.erase(it);
        return true;
    }

    void PluginManager::UnloadAll() {
        std::lock_guard<std::mutex> lock(m_Mutex);

        for (auto it = m_Plugins.begin(); it != m_Plugins.end(); ) {
            auto& plugin = it->second;
            // Force destroy modules if unloading all on shutdown
            if (plugin.info.DestroyModuleFn) {
                for (auto* mod : plugin.activeModules) {
                    plugin.info.DestroyModuleFn(mod);
                }
            }
            plugin.activeModules.clear();

            plugin.handle.Unload();
            it = m_Plugins.erase(it);
        }
    }

    bool PluginManager::IsPluginLoaded(const std::string& path) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Plugins.find(path) != m_Plugins.end();
    }

} // namespace eng::runtime
