#include "Runtime/Public/World/WorldManager.h"
#include "Runtime/Public/World/WorldFileReader.h"
#include "Runtime/Public/World/WorldZoneReader.h"
#include "RenderingEngine/Public/IAssetManager.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Core/Logger.h"

#include "Scene/Scene.h"
#include "Scene/SceneLoader.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneObject.h"
#include "ECS/PlayerControllerSystem.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"
#include "Runtime/Public/RuntimeContext.h"
#include "EventManagement/GameEvent.h"
#include "EventManagement/SceneEventTypes.h"
#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include "Runtime/Public/Gameplay/GameplayEventBus.h"
#include "Core/World.h"

#include "Renderer/scene/Texture.h"
#include "Renderer/scene/Mesh.h"
#include "Renderer/scene/Material.h"

namespace Omnix
{
    WorldManager::WorldManager(eng::runtime::IAssetManager* assets, eng::runtime::AssetRegistry* assetRegistry, eng::runtime::ISceneManager* scenes)
        : m_Assets(assets)
        , m_Registry(assetRegistry)
        , m_Scenes(scenes)
        , m_HasActiveWorld(false)
        , m_ActiveZoneUUIDHigh(0)
        , m_ActiveZoneUUIDLow(0)
        , m_PreviousZoneUUIDHigh(0)
        , m_PreviousZoneUUIDLow(0)
    {
    }

    WorldManager::~WorldManager()
    {
        UnloadWorld();
    }

    WorldFileResult WorldManager::LoadWorld(const std::filesystem::path& path)
    {
        // 1. Unload existing world cleanly first
        UnloadWorld();

        if (!m_Registry)
        {
            LOG_ERROR("[WorldManager] AssetRegistry is null during LoadWorld!");
            return WorldFileResult::Fail(WorldFileError::CorruptedFile);
        }

        // 2. Add world asset lookup through AssetRegistry
        m_Registry->RegisterAsset(path.string(), AssetType::Unknown);

        // 3. Load the .omnixworld file
        WorldFileResult readRes = WorldFileReader::ReadFromFile(path, m_ActiveWorld);
        if (!readRes.Success())
        {
            LOG_ERROR("[WorldManager] Failed to read world file: %s (Error: %d)", path.string().c_str(), static_cast<int>(readRes.error));
            return readRes;
        }

        m_HasActiveWorld = true;
        LOG_INFO("[WorldManager] Loaded active world: %s (Zones: %u, Dependencies: %u)", 
                 m_ActiveWorld.worldName.c_str(), 
                 static_cast<uint32_t>(m_ActiveWorld.zones.size()),
                 static_cast<uint32_t>(m_ActiveWorld.dependencies.size()));

        // 4. Resolve zone scene references and dependencies
        m_LoadedZones.reserve(m_ActiveWorld.zones.size());
        for (const auto& zoneEntry : m_ActiveWorld.zones)
        {
            WorldZone zone;
            WorldFileResult zoneRes = WorldZoneReader::ReadFromFile(zoneEntry.zonePath, zone);
            if (!zoneRes.Success())
            {
                LOG_WARN("[WorldManager] Failed to read zone file: %s (Error: %d)", zoneEntry.zonePath, static_cast<int>(zoneRes.error));
                // Continue loading other zones rather than failing the whole world load
                continue;
            }

            // Resolve scene reference through AssetManager
            if (!zone.sceneAssetPath.empty())
            {
                m_Registry->RegisterAsset(zone.sceneAssetPath, AssetType::Scene);
                if (m_Assets)
                {
                    m_Assets->Load<Scene>(zone.sceneAssetPath);
                }

                // Instantiate zone scene entities
                SceneManager* sceneMgr = m_Scenes ? dynamic_cast<SceneManager*>(m_Scenes) : nullptr;
                if (sceneMgr)
                {
                    Scene* activeScene = sceneMgr->GetActiveScene();
                    Coordinator* coordinator = &sceneMgr->GetCoordinator();
                    if (activeScene && coordinator)
                    {
                        Scene* zoneScene = SceneLoader::LoadFromFile(zone.sceneAssetPath);
                        if (zoneScene)
                        {
                            const auto& objects = zoneScene->GetAllSceneObjects();
                            for (const auto& obj : objects)
                            {
                                if (obj)
                                {
                                    // Add to active scene
                                    activeScene->AddSceneObject(obj);

                                    // Initialize in ECS coordinator
                                    obj->InitializeWithECS(*coordinator);

                                    // Track loaded entity ID
                                    uint32_t ent = obj->GetECSEntity();
                                    if (ent != INVALID_ENTITY)
                                    {
                                        m_ZoneEntities.push_back(ent);

                                        // Add ZoneEntityComponent
                                        eng::runtime::ZoneEntityComponent zec;
                                        zec.zoneUUIDHigh = zone.zoneUUIDHigh;
                                        zec.zoneUUIDLow = zone.zoneUUIDLow;
                                        zec.simulating = false; // Keep inactive zones loaded but non-simulating initially
                                        coordinator->AddComponent<eng::runtime::ZoneEntityComponent>(ent, zec);
                                    }
                                }
                            }
                            delete zoneScene;
                        }
                    }
                }
            }

            // Resolve zone asset dependencies through AssetManager
            for (const auto& dep : zone.assetDependencies)
            {
                AssetType type = static_cast<AssetType>(dep.assetType);
                m_Registry->RegisterAsset(dep.assetPath, type);
                if (m_Assets)
                {
                    switch (type)
                    {
                        case AssetType::Texture:
                            m_Assets->Load<eng::renderer::Texture>(dep.assetPath);
                            break;
                        case AssetType::Mesh:
                            m_Assets->Load<eng::renderer::Mesh>(dep.assetPath);
                            break;
                        case AssetType::Material:
                            m_Assets->Load<eng::renderer::Material>(dep.assetPath);
                            break;
                        default:
                            m_Assets->Load<void>(dep.assetPath);
                            break;
                    }
                }
            }

            zone.state = ZoneState::Inactive;
            m_LoadedZones.push_back(zone);
        }

        return WorldFileResult::Ok();
    }

    void WorldZoneCleanupHelper(WorldZone& zone, eng::runtime::IAssetManager* assets)
    {
        if (assets)
        {
            // Clean up resources if necessary
        }
    }

    void WorldManager::UnloadWorld()
    {
        if (!m_HasActiveWorld)
        {
            return;
        }

        LOG_INFO("[WorldManager] Unloading active world: %s", m_ActiveWorld.worldName.c_str());

        // Destroy all zone entities in ECS coordinator
        SceneManager* sceneMgr = m_Scenes ? dynamic_cast<SceneManager*>(m_Scenes) : nullptr;
        if (sceneMgr)
        {
            Coordinator* coordinator = &sceneMgr->GetCoordinator();
            Scene* activeScene = sceneMgr->GetActiveScene();
            if (coordinator && activeScene)
            {
                for (uint32_t ent : m_ZoneEntities)
                {
                    if (ent != INVALID_ENTITY)
                    {
                        SceneObject* obj = sceneMgr->GetSceneObjectByID(ent);
                        if (obj)
                        {
                            std::shared_ptr<SceneObject> sharedObj;
                            for (const auto& o : activeScene->GetAllSceneObjects())
                            {
                                if (o.get() == obj)
                                {
                                    sharedObj = o;
                                    break;
                                }
                            }
                            if (sharedObj)
                            {
                                activeScene->RemoveSceneObject(sharedObj);
                            }
                        }

                        if (coordinator->IsEntityAlive(ent))
                        {
                            coordinator->DestroyEntity(ent);
                        }
                    }
                }
            }
        }
        m_ZoneEntities.clear();

        // Cleanup zones
        for (auto& zone : m_LoadedZones)
        {
            WorldZoneCleanupHelper(zone, m_Assets);
        }
        m_LoadedZones.clear();

        // Reset world descriptor
        m_ActiveWorld.worldUUIDHigh = 0;
        m_ActiveWorld.worldUUIDLow = 0;
        m_ActiveWorld.worldName.clear();
        m_ActiveWorld.settings = WorldSettingsBlock();
        m_ActiveWorld.entryPoint = WorldEntryPoint();
        m_ActiveWorld.zones.clear();
        m_ActiveWorld.dependencies.clear();

        m_ActiveZoneUUIDHigh = 0;
        m_ActiveZoneUUIDLow = 0;
        m_PreviousZoneUUIDHigh = 0;
        m_PreviousZoneUUIDLow = 0;

        m_HasActiveWorld = false;
        LOG_INFO("[WorldManager] Active world unloaded cleanly.");
    }

    void WorldManager::Update(eng::runtime::RuntimeContext& context, float deltaTime)
    {
        if (!m_HasActiveWorld)
        {
            return;
        }

        // 1. Get the player entity and position
        Entity playerEnt = INVALID_ENTITY;
        if (context.ecs) {
            auto* world = dynamic_cast<eng::runtime::World*>(context.ecs);
            if (world) {
                if (auto playerControllerSys = world->GetSystem<eng::runtime::PlayerControllerSystem>()) {
                    Entity p = playerControllerSys->GetPlayerEntity();
                    if (p != INVALID_ENTITY && context.ecs->getCoordinator().IsEntityAlive(p)) {
                        playerEnt = p;
                    }
                }
            }
        }
        if (playerEnt == INVALID_ENTITY && context.ecs) {
            auto& coordinator = context.ecs->getCoordinator();
            auto cccType = coordinator.GetComponentType<CharacterControllerComponent>();
            auto transformType = coordinator.GetComponentType<TransformComponent>();
            for (Entity ent : coordinator.GetActiveEntities()) {
                if (coordinator.IsEntityAlive(ent)) {
                    auto sig = coordinator.GetSignature(ent);
                    if (sig.test(cccType) && sig.test(transformType)) {
                        playerEnt = ent;
                        break;
                    }
                }
            }
        }

        if (playerEnt == INVALID_ENTITY)
        {
            return;
        }

        auto& coordinator = context.ecs->getCoordinator();
        const auto& transform = coordinator.GetComponent<TransformComponent>(playerEnt);
        Vec3 playerPos = { transform.position.x, transform.position.y, transform.position.z };

        // 2. Detect player current zone
        WorldZone* newActiveZone = nullptr;
        for (auto& zone : m_LoadedZones)
        {
            if (playerPos.x >= zone.bounds.min.x && playerPos.x <= zone.bounds.max.x &&
                playerPos.y >= zone.bounds.min.y && playerPos.y <= zone.bounds.max.y &&
                playerPos.z >= zone.bounds.min.z && playerPos.z <= zone.bounds.max.z)
            {
                newActiveZone = &zone;
                break;
            }
        }

        uint64_t newActiveHigh = newActiveZone ? newActiveZone->zoneUUIDHigh : 0;
        uint64_t newActiveLow = newActiveZone ? newActiveZone->zoneUUIDLow : 0;

        if (newActiveHigh != m_ActiveZoneUUIDHigh || newActiveLow != m_ActiveZoneUUIDLow)
        {
            LOG_INFO("[WorldManager] Zone transition: Active Zone changed from [%llu, %llu] to [%llu, %llu]",
                     m_ActiveZoneUUIDHigh, m_ActiveZoneUUIDLow, newActiveHigh, newActiveLow);

            m_PreviousZoneUUIDHigh = m_ActiveZoneUUIDHigh;
            m_PreviousZoneUUIDLow = m_ActiveZoneUUIDLow;

            // 3. Deactivate previous zone
            if (m_ActiveZoneUUIDHigh != 0 || m_ActiveZoneUUIDLow != 0)
            {
                for (auto& zone : m_LoadedZones)
                {
                    if (zone.zoneUUIDHigh == m_ActiveZoneUUIDHigh && zone.zoneUUIDLow == m_ActiveZoneUUIDLow)
                    {
                        zone.state = ZoneState::Inactive;
                        break;
                    }
                }

                if (context.events)
                {
                    context.events->queueEvent(std::make_unique<Omnix::ZoneExitEvent>(
                        m_ActiveZoneUUIDHigh, m_ActiveZoneUUIDLow, playerEnt));
                }
                if (context.gameplayEventBus)
                {
                    eng::runtime::GameplayEvent gpEvent;
                    gpEvent.Type = eng::runtime::GameplayEventType::ZoneExit;
                    gpEvent.Source = playerEnt;
                    gpEvent.ZoneUUIDHigh = m_ActiveZoneUUIDHigh;
                    gpEvent.ZoneUUIDLow = m_ActiveZoneUUIDLow;
                    context.gameplayEventBus->QueueEvent(gpEvent);
                }
            }

            m_ActiveZoneUUIDHigh = newActiveHigh;
            m_ActiveZoneUUIDLow = newActiveLow;

            // 4. Activate new zone
            if (m_ActiveZoneUUIDHigh != 0 || m_ActiveZoneUUIDLow != 0)
            {
                if (newActiveZone)
                {
                    newActiveZone->state = ZoneState::Active;
                }

                if (context.events)
                {
                    context.events->queueEvent(std::make_unique<Omnix::ZoneEnterEvent>(
                        m_ActiveZoneUUIDHigh, m_ActiveZoneUUIDLow, playerEnt));
                }
                if (context.gameplayEventBus)
                {
                    eng::runtime::GameplayEvent gpEvent;
                    gpEvent.Type = eng::runtime::GameplayEventType::ZoneEnter;
                    gpEvent.Source = playerEnt;
                    gpEvent.ZoneUUIDHigh = m_ActiveZoneUUIDHigh;
                    gpEvent.ZoneUUIDLow = m_ActiveZoneUUIDLow;
                    context.gameplayEventBus->QueueEvent(gpEvent);
                }
            }

            // 5. Update simulating flag on zone entities
            auto zoneEntityCompType = coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>();
            for (Entity ent : coordinator.GetActiveEntities())
            {
                if (coordinator.IsEntityAlive(ent))
                {
                    auto sig = coordinator.GetSignature(ent);
                    if (sig.test(zoneEntityCompType))
                    {
                        auto& zec = coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(ent);
                        zec.simulating = (zec.zoneUUIDHigh == m_ActiveZoneUUIDHigh && zec.zoneUUIDLow == m_ActiveZoneUUIDLow);
                    }
                }
            }
        }
    }

    const WorldDescriptor* WorldManager::GetActiveWorld() const
    {
        return m_HasActiveWorld ? &m_ActiveWorld : nullptr;
    }

    bool WorldManager::HasActiveWorld() const
    {
        return m_HasActiveWorld;
    }

    const std::vector<WorldZone>& WorldManager::GetLoadedZones() const
    {
        return m_LoadedZones;
    }
}
