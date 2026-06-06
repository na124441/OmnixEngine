#pragma once

#include "Runtime/Public/World/WorldDescriptor.h"
#include "Runtime/Public/World/WorldZone.h"
#include "Runtime/Public/World/WorldFileResult.h"
#include <filesystem>
#include <vector>

namespace eng::runtime
{
    class IAssetManager;
    class AssetRegistry;
    class ISceneManager;
    struct RuntimeContext;
}

namespace Omnix
{
    class WorldManager
    {
    public:
        WorldManager(eng::runtime::IAssetManager* assets, eng::runtime::AssetRegistry* assetRegistry, eng::runtime::ISceneManager* scenes = nullptr);
        ~WorldManager();

        WorldFileResult LoadWorld(const std::filesystem::path& path);
        void UnloadWorld();

        void Update(eng::runtime::RuntimeContext& context, float deltaTime);

        const WorldDescriptor* GetActiveWorld() const;
        bool HasActiveWorld() const;
        const std::vector<WorldZone>& GetLoadedZones() const;

        uint64_t GetActiveZoneUUIDHigh() const { return m_ActiveZoneUUIDHigh; }
        uint64_t GetActiveZoneUUIDLow() const { return m_ActiveZoneUUIDLow; }
        uint64_t GetPreviousZoneUUIDHigh() const { return m_PreviousZoneUUIDHigh; }
        uint64_t GetPreviousZoneUUIDLow() const { return m_PreviousZoneUUIDLow; }

    private:
        eng::runtime::IAssetManager* m_Assets;
        eng::runtime::AssetRegistry* m_Registry;
        eng::runtime::ISceneManager* m_Scenes;

        WorldDescriptor m_ActiveWorld;
        bool m_HasActiveWorld;

        std::vector<WorldZone> m_LoadedZones;

        uint64_t m_ActiveZoneUUIDHigh = 0;
        uint64_t m_ActiveZoneUUIDLow = 0;
        uint64_t m_PreviousZoneUUIDHigh = 0;
        uint64_t m_PreviousZoneUUIDLow = 0;

        // Tracks all entities loaded from zone scenes to destroy them on UnloadWorld()
        std::vector<uint32_t> m_ZoneEntities;
    };
}

