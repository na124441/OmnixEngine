#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
using HMODULE = void*;
#endif

namespace eng::runtime {

    class IModule;

    constexpr uint32_t ENGINE_ABI_VERSION = 0x00010000; // v1.0.0 (ABI major/minor/patch)

    struct PluginInfo {
        uint32_t abiVersion = 0;
        const char* name = nullptr;
        const char* version = nullptr;
        IModule* (*CreateModuleFn)() = nullptr;
        void (*DestroyModuleFn)(IModule*) = nullptr;
    };

    using GetPluginInfoFn = PluginInfo (*)();

    struct LoadedPlugin {
        std::string path;
        HMODULE handle = nullptr;
        PluginInfo info;
        // Track instantiated modules to prevent hot-unload if there are active references
        std::vector<IModule*> activeModules;
    };

    /**
     * @class PluginManager
     * @brief Manages loading, ABI verification, module instantiation, and safe unloading of dynamic libraries.
     */
    class PluginManager {
    public:
        PluginManager() = default;
        ~PluginManager() { UnloadAll(); }

        PluginManager(const PluginManager&) = delete;
        PluginManager& operator=(const PluginManager&) = delete;

        /**
         * @brief Load a plugin from a dynamic library file path (.dll).
         * @return Pointer to loaded IModule instance, or nullptr on failure.
         */
        IModule* LoadPlugin(const std::string& path);

        /**
         * @brief Unloads a plugin if there are no live references.
         * @return True if unloaded successfully, false if refused due to hot-unload safety.
         */
        bool UnloadPlugin(const std::string& path);

        /**
         * @brief Unloads all loaded plugins.
         */
        void UnloadAll();

        /**
         * @brief Queries whether a plugin is loaded.
         */
        [[nodiscard]] bool IsPluginLoaded(const std::string& path) const;

    private:
        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, LoadedPlugin> m_Plugins;
    };

} // namespace eng::runtime
