#pragma once

#include "Runtime/World/WorldDescriptor.h"
#include "Runtime/World/WorldZone.h"
#include "Runtime/World/WorldFileResult.h"
#include <filesystem>
#include <vector>

namespace eng::core
{
    struct RuntimeContext;
}

namespace eng::runtime
{
    class IAssetManager;
    class AssetRegistry;
    class ISceneManager;
    using eng::core::RuntimeContext;
}

class Coordinator;

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

        // Week 6 spatial queries
        std::vector<uint32_t> QueryEntitiesInZone(uint64_t high, uint64_t low, Coordinator& coordinator) const;
        std::vector<uint32_t> QueryActiveZoneEntities(Coordinator& coordinator) const;
        std::vector<WorldZone> QueryNearbyZones(const Vec3& position, float radius) const;
        std::vector<uint32_t> QueryNeighboringZoneEntities(uint64_t high, uint64_t low, Coordinator& coordinator) const;

        // Diagnostics
        void PrintZoneMembershipDiagnostics(Coordinator& coordinator) const;

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

namespace eng::runtime {
    using WorldManager = ::Omnix::WorldManager;
}

