#pragma once
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetRegistry.h"
#include "Runtime/AssetManager.h"
#include "Runtime/FileWatcher.h"
#include "Runtime/ReloadEvent.h"
#include "Runtime/ReloadState.h"
#include <unordered_map>
#include <set>
#include <vector>
#include <string>
#include <queue>

namespace eng::runtime {

    class HotReloadSystem
    {
    public:
        HotReloadSystem(AssetRegistry& registry, AssetManager& assetManager);
        ~HotReloadSystem() = default;

        // Prevent copying
        HotReloadSystem(const HotReloadSystem&) = delete;
        HotReloadSystem& operator=(const HotReloadSystem&) = delete;

        /**
         * @brief Registers a directory to watch for source changes.
         */
        void WatchDirectory(const std::filesystem::path& path);

        /**
         * @brief Polls the watcher for changes and processes the reload queue.
         */
        void Poll();

        /**
         * @brief Force reloads a specific asset handle immediately.
         */
        bool ReloadAsset(AssetHandle handle);

        /**
         * @brief Queues an asset handle for asynchronous reloading.
         */
        void QueueReload(AssetHandle handle);

        /**
         * @brief Returns reload events for diagnostics.
         */
        const std::vector<ReloadEvent>& GetHistory() const { return m_History; }

        /**
         * @brief Clears history logs.
         */
        void ClearHistory() { m_History.clear(); }

    private:
        void HandleFileChange(const std::filesystem::path& path);
        void RebuildDependentMappings();
        void GatherDependentsTopological(AssetHandle handle, std::vector<AssetHandle>& outOrder, std::set<AssetHandle>& visited, std::set<AssetHandle>& recursionStack);

        bool ReimportAsset(const AssetMetadata& metadata, std::string& outError);
        bool SwapRuntimeAsset(const AssetMetadata& metadata);

        AssetRegistry& m_Registry;
        AssetManager& m_AssetManager;
        FileWatcher m_Watcher;

        std::queue<AssetHandle> m_ReloadQueue;
        std::unordered_map<AssetHandle, std::vector<AssetHandle>> m_Dependents;
        std::unordered_map<AssetHandle, ReloadState> m_ReloadStates;

        std::vector<ReloadEvent> m_History;
    };

} // namespace eng::runtime
