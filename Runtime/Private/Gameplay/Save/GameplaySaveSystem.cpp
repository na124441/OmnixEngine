#include "Runtime/Public/Gameplay/Save/GameplaySaveSystem.h"
#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Gameplay/GameMode.h"
#include "Runtime/Public/Gameplay/PlayerStateComponent.h"
#include "Runtime/Public/Gameplay/Objectives/ObjectiveSystem.h"
#include "Runtime/Public/Gameplay/StateObjects/SimpleStateComponent.h"
#include "Runtime/Public/Gameplay/StateObjects/ActivatableComponent.h"
#include "Runtime/Public/Gameplay/StateObjects/DoorComponent.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Core/Logging/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <cstring>

namespace eng::runtime {

    // FNV-1a 64-bit algorithm for checksumming
    static uint64_t FNV1a_64(const void* data, size_t size)
    {
        const uint64_t fnv_prime = 1099511628211ULL;
        const uint64_t fnv_offset_basis = 14695981039346656037ULL;

        uint64_t hash = fnv_offset_basis;
        const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= byte_ptr[i];
            hash *= fnv_prime;
        }
        return hash;
    }

    // Binary Serialization Helpers
    template<typename T>
    static void WritePod(std::ostream& os, const T& val)
    {
        os.write(reinterpret_cast<const char*>(&val), sizeof(T));
    }

    template<typename T>
    static bool ReadPod(std::istream& is, T& val)
    {
        is.read(reinterpret_cast<char*>(&val), sizeof(T));
        return !is.fail();
    }

    static void WriteString(std::ostream& os, const std::string& str)
    {
        uint32_t len = static_cast<uint32_t>(str.size());
        WritePod(os, len);
        if (len > 0)
        {
            os.write(str.data(), len);
        }
    }

    static bool ReadString(std::istream& is, std::string& str, uint32_t maxLength = 4096)
    {
        uint32_t len = 0;
        if (!ReadPod(is, len)) return false;
        if (len > maxLength)
        {
            LOG_ERROR("[SaveSystem] String length %u exceeds safety limit %u", len, maxLength);
            return false;
        }
        str.resize(len);
        if (len > 0)
        {
            is.read(&str[0], len);
        }
        return !is.fail();
    }

    static void WriteStringVector(std::ostream& os, const std::vector<std::string>& vec)
    {
        uint32_t count = static_cast<uint32_t>(vec.size());
        WritePod(os, count);
        for (const auto& str : vec)
        {
            WriteString(os, str);
        }
    }

    static bool ReadStringVector(std::istream& is, std::vector<std::string>& vec, uint32_t maxCount = 1024)
    {
        uint32_t count = 0;
        if (!ReadPod(is, count)) return false;
        if (count > maxCount)
        {
            LOG_ERROR("[SaveSystem] Vector size %u exceeds safety limit %u", count, maxCount);
            return false;
        }
        vec.resize(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!ReadString(is, vec[i])) return false;
        }
        return true;
    }

    static void WriteInteractableStates(std::ostream& os, const std::unordered_map<std::string, bool>& map)
    {
        // Determinism: sort map keys first
        std::vector<std::string> keys;
        keys.reserve(map.size());
        for (const auto& pair : map)
        {
            keys.push_back(pair.first);
        }
        std::sort(keys.begin(), keys.end());

        uint32_t count = static_cast<uint32_t>(keys.size());
        WritePod(os, count);
        for (const auto& key : keys)
        {
            WriteString(os, key);
            bool value = map.at(key);
            WritePod(os, value);
        }
    }

    static bool ReadInteractableStates(std::istream& is, std::unordered_map<std::string, bool>& map, uint32_t maxCount = 10000)
    {
        uint32_t count = 0;
        if (!ReadPod(is, count)) return false;
        if (count > maxCount)
        {
            LOG_ERROR("[SaveSystem] Interactable state count %u exceeds safety limit %u", count, maxCount);
            return false;
        }
        map.clear();
        for (uint32_t i = 0; i < count; ++i)
        {
            std::string key;
            if (!ReadString(is, key)) return false;
            bool value = false;
            if (!ReadPod(is, value)) return false;
            map[key] = value;
        }
        return true;
    }

    static void WriteSimpleObjectStates(std::ostream& os, const std::unordered_map<std::string, SimpleObjectState>& map)
    {
        // Determinism: sort map keys first
        std::vector<std::string> keys;
        keys.reserve(map.size());
        for (const auto& pair : map)
        {
            keys.push_back(pair.first);
        }
        std::sort(keys.begin(), keys.end());

        uint32_t count = static_cast<uint32_t>(keys.size());
        WritePod(os, count);
        for (const auto& key : keys)
        {
            WriteString(os, key);
            int32_t value = static_cast<int32_t>(map.at(key));
            WritePod(os, value);
        }
    }

    static bool ReadSimpleObjectStates(std::istream& is, std::unordered_map<std::string, SimpleObjectState>& map, uint32_t maxCount = 10000)
    {
        uint32_t count = 0;
        if (!ReadPod(is, count)) return false;
        if (count > maxCount)
        {
            LOG_ERROR("[SaveSystem] Simple state object count %u exceeds safety limit %u", count, maxCount);
            return false;
        }
        map.clear();
        for (uint32_t i = 0; i < count; ++i)
        {
            std::string key;
            if (!ReadString(is, key)) return false;
            int32_t valInt = 0;
            if (!ReadPod(is, valInt)) return false;
            if (valInt < 0 || valInt > 4)
            {
                LOG_ERROR("[SaveSystem] Invalid SimpleObjectState value: %d", valInt);
                return false;
            }
            map[key] = static_cast<SimpleObjectState>(valInt);
        }
        return true;
    }


    GameplaySaveSystem::GameplaySaveSystem() = default;
    GameplaySaveSystem::~GameplaySaveSystem() = default;

    void GameplaySaveSystem::Initialize(RuntimeContext* context)
    {
        m_Context = context;
    }

    GameplaySaveSnapshot GameplaySaveSystem::CaptureSnapshot()
    {
        GameplaySaveSnapshot snapshot;
        snapshot.Version = 1;

        if (!m_Context)
        {
            snapshot.Valid = false;
            return snapshot;
        }

        // 1. Scene Name
        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        if (sceneMgr && sceneMgr->GetActiveScene())
        {
            snapshot.SceneName = sceneMgr->GetActiveScene()->GetName();
        }

        // 2. Player Transform, Health, Alive State
        if (m_Context->gameMode && m_Context->ecs)
        {
            Entity player = m_Context->gameMode->FindPlayerEntity();
            if (player != INVALID_ENTITY)
            {
                auto& coordinator = m_Context->ecs->getCoordinator();
                auto transformType = coordinator.GetComponentType<TransformComponent>();
                if (coordinator.GetSignature(player).test(transformType))
                {
                    snapshot.PlayerTransform = coordinator.GetComponent<TransformComponent>(player);
                }
                auto pscType = coordinator.GetComponentType<PlayerStateComponent>();
                if (coordinator.GetSignature(player).test(pscType))
                {
                    const auto& psc = coordinator.GetComponent<PlayerStateComponent>(player);
                    snapshot.PlayerHealth = psc.Health;
                    snapshot.PlayerAlive = psc.IsAlive;
                }
            }
        }

        // 3. Objectives & Checkpoints
        if (m_Context->gameMode)
        {
            const auto& gs = m_Context->gameMode->GetGameState();
            snapshot.ActiveObjectiveID = gs.ActiveObjectiveID;
            snapshot.CompletedObjectives = gs.CompletedObjectives;
            snapshot.CheckpointID = gs.CurrentCheckpointID;
        }

        // 4. State Objects & Activatable / Interactable States
        if (m_Context->ecs)
        {
            auto& coordinator = m_Context->ecs->getCoordinator();
            auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
            auto activatableType = coordinator.GetComponentType<ActivatableComponent>();

            for (Entity ent : coordinator.GetActiveEntities())
            {
                if (ent != INVALID_ENTITY && coordinator.IsEntityAlive(ent))
                {
                    auto sig = coordinator.GetSignature(ent);
                    if (sig.test(activatableType))
                    {
                        const auto& act = coordinator.GetComponent<ActivatableComponent>(ent);
                        if (!act.ActivationID.empty())
                        {
                            snapshot.InteractableActivationStates[act.ActivationID] = act.HasActivated;
                        }
                    }
                    if (sig.test(stateType) && sig.test(activatableType))
                    {
                        const auto& act = coordinator.GetComponent<ActivatableComponent>(ent);
                        if (!act.ActivationID.empty())
                        {
                            const auto& state = coordinator.GetComponent<SimpleStateComponent>(ent);
                            snapshot.SimpleObjectStates[act.ActivationID] = state.CurrentState;
                        }
                    }
                }
            }
        }

        // 5. Compute checksum
        snapshot.Checksum = ComputeChecksum(snapshot);
        snapshot.Valid = true;

        return snapshot;
    }

    uint64_t GameplaySaveSystem::ComputeChecksum(const GameplaySaveSnapshot& snapshot)
    {
        // Serialize the payload parts of the snapshot in a deterministic binary manner
        std::stringstream ss(std::ios::out | std::ios::binary);

        WriteString(ss, snapshot.SceneName);
        WritePod(ss, snapshot.PlayerTransform);
        WritePod(ss, snapshot.PlayerHealth);
        WritePod(ss, snapshot.PlayerAlive);
        WriteString(ss, snapshot.ActiveObjectiveID);
        WriteStringVector(ss, snapshot.CompletedObjectives);
        WriteString(ss, snapshot.CheckpointID);
        WriteInteractableStates(ss, snapshot.InteractableActivationStates);
        WriteSimpleObjectStates(ss, snapshot.SimpleObjectStates);

        std::string payload = ss.str();
        return FNV1a_64(payload.data(), payload.size());
    }

    bool GameplaySaveSystem::SaveToFile(const std::string& path, const GameplaySaveSnapshot& snapshot)
    {
        m_LastSavePath = path;
        m_LastSaveValid = false;

        if (!snapshot.Valid)
        {
            LOG_ERROR("[SaveSystem] Cannot save an invalid snapshot to file: %s", path.c_str());
            return false;
        }

        // Create parent directories if they don't exist
        std::filesystem::path fsPath(path);
        if (fsPath.has_parent_path())
        {
            std::filesystem::create_directories(fsPath.parent_path());
        }

        std::ofstream os(path, std::ios::out | std::ios::binary);
        if (!os.is_open())
        {
            LOG_ERROR("[SaveSystem] Failed to open save file for writing: %s", path.c_str());
            return false;
        }

        // 1. Serialize payload into memory buffer to get exact payload size
        std::stringstream ss(std::ios::out | std::ios::binary);
        WriteString(ss, snapshot.SceneName);
        WritePod(ss, snapshot.PlayerTransform);
        WritePod(ss, snapshot.PlayerHealth);
        WritePod(ss, snapshot.PlayerAlive);
        WriteString(ss, snapshot.ActiveObjectiveID);
        WriteStringVector(ss, snapshot.CompletedObjectives);
        WriteString(ss, snapshot.CheckpointID);
        WriteInteractableStates(ss, snapshot.InteractableActivationStates);
        WriteSimpleObjectStates(ss, snapshot.SimpleObjectStates);

        std::string payload = ss.str();

        // 2. Write Header
        GameplaySaveHeader header;
        std::memcpy(header.Magic, "OMNSAVE\0", 8);
        header.Version = snapshot.Version;
        header.PayloadSize = payload.size();
        header.Checksum = snapshot.Checksum;

        os.write(reinterpret_cast<const char*>(&header), sizeof(GameplaySaveHeader));
        if (os.fail())
        {
            LOG_ERROR("[SaveSystem] Failed to write header to file: %s", path.c_str());
            return false;
        }

        // 3. Write Payload
        os.write(payload.data(), payload.size());
        if (os.fail())
        {
            LOG_ERROR("[SaveSystem] Failed to write payload to file: %s", path.c_str());
            return false;
        }

        // 4. Write Checksum at the end
        WritePod(os, snapshot.Checksum);
        if (os.fail())
        {
            LOG_ERROR("[SaveSystem] Failed to write trailing checksum to file: %s", path.c_str());
            return false;
        }

        os.close();

        // Update Diagnostics
        m_LastSaveValid = true;
        m_LastSaveVersion = snapshot.Version;
        m_LastSaveScene = snapshot.SceneName;
        m_LastSaveChecksum = snapshot.Checksum;
        m_LastSavePayloadSize = payload.size();
        m_LastSavePlayerTransform = snapshot.PlayerTransform;
        m_LastSavePlayerHealth = snapshot.PlayerHealth;
        m_LastSavePlayerAlive = snapshot.PlayerAlive;
        m_LastSaveActiveObjectiveID = snapshot.ActiveObjectiveID;
        m_LastSaveCompletedObjectivesCount = snapshot.CompletedObjectives.size();
        m_LastSaveCheckpointID = snapshot.CheckpointID;
        m_LastSaveInteractableStatesCount = snapshot.InteractableActivationStates.size();
        m_LastSaveSimpleObjectStatesCount = snapshot.SimpleObjectStates.size();

        LOG_INFO("[SaveSystem] Successfully wrote save to file '%s' (Size: %zu bytes, Checksum: 0x%llX)", 
            path.c_str(), sizeof(GameplaySaveHeader) + payload.size() + sizeof(uint64_t), snapshot.Checksum);

        return true;
    }

    bool GameplaySaveSystem::LoadFromFile(const std::string& path, GameplaySaveSnapshot& outSnapshot)
    {
        std::ifstream is(path, std::ios::in | std::ios::binary);
        if (!is.is_open())
        {
            LOG_ERROR("[SaveSystem] Failed to open save file for reading: %s", path.c_str());
            return false;
        }

        // 1. Read Header
        GameplaySaveHeader header;
        is.read(reinterpret_cast<char*>(&header), sizeof(GameplaySaveHeader));
        if (is.fail())
        {
            LOG_ERROR("[SaveSystem] Failed to read header from save file: %s", path.c_str());
            return false;
        }

        // 2. Validate Magic
        if (std::memcmp(header.Magic, "OMNSAVE\0", 8) != 0)
        {
            LOG_ERROR("[SaveSystem] Invalid magic header in save file: %s", path.c_str());
            return false;
        }

        // 3. Validate Version
        if (header.Version != 1)
        {
            LOG_ERROR("[SaveSystem] Unsupported save version: %u", header.Version);
            return false;
        }

        // 4. Safety check payload size
        const uint64_t MaxSavePayloadSize = 16 * 1024 * 1024; // 16 MB limit
        if (header.PayloadSize > MaxSavePayloadSize)
        {
            LOG_ERROR("[SaveSystem] Save payload size too large: %llu bytes", header.PayloadSize);
            return false;
        }

        // 5. Read Payload
        std::vector<char> payloadBuffer(header.PayloadSize);
        if (header.PayloadSize > 0)
        {
            is.read(payloadBuffer.data(), header.PayloadSize);
            if (is.fail())
            {
                LOG_ERROR("[SaveSystem] Failed to read payload of size %llu bytes from file: %s", header.PayloadSize, path.c_str());
                return false;
            }
        }

        // 6. Read Trailing Checksum
        uint64_t trailingChecksum = 0;
        ReadPod(is, trailingChecksum);
        if (is.fail())
        {
            LOG_ERROR("[SaveSystem] Failed to read trailing checksum from file: %s (likely truncated save)", path.c_str());
            return false;
        }

        is.close();

        // 7. Verify Checksums
        uint64_t computedChecksum = FNV1a_64(payloadBuffer.data(), payloadBuffer.size());
        if (computedChecksum != header.Checksum)
        {
            LOG_ERROR("[SaveSystem] Header checksum mismatch. Saved: 0x%llX, Computed: 0x%llX", header.Checksum, computedChecksum);
            return false;
        }

        if (computedChecksum != trailingChecksum)
        {
            LOG_ERROR("[SaveSystem] Trailing checksum mismatch. Saved: 0x%llX, Computed: 0x%llX", trailingChecksum, computedChecksum);
            return false;
        }

        // 8. Deserialize Payload from memory stream
        std::string payloadStr(payloadBuffer.begin(), payloadBuffer.end());
        std::stringstream ss(payloadStr, std::ios::in | std::ios::binary);

        GameplaySaveSnapshot snapshot;
        snapshot.Version = header.Version;

        if (!ReadString(ss, snapshot.SceneName)) return false;
        if (!ReadPod(ss, snapshot.PlayerTransform)) return false;
        if (!ReadPod(ss, snapshot.PlayerHealth)) return false;
        if (!ReadPod(ss, snapshot.PlayerAlive)) return false;
        if (!ReadString(ss, snapshot.ActiveObjectiveID)) return false;
        if (!ReadStringVector(ss, snapshot.CompletedObjectives)) return false;
        if (!ReadString(ss, snapshot.CheckpointID)) return false;
        if (!ReadInteractableStates(ss, snapshot.InteractableActivationStates)) return false;
        if (!ReadSimpleObjectStates(ss, snapshot.SimpleObjectStates)) return false;

        snapshot.Checksum = computedChecksum;
        snapshot.Valid = true;

        outSnapshot = snapshot;
        return true;
    }

    bool GameplaySaveSystem::RestoreSnapshot(const GameplaySaveSnapshot& snapshot)
    {
        m_LastRestoreSourceSave = m_LastSavePath;
        m_LastRestoreResult = "Failed (Invalid)";
        m_LastRestoreSceneMatch = false;
        m_LastRestoreChecksumValid = false;

        if (!snapshot.Valid)
        {
            LOG_ERROR("[SaveSystem] Cannot restore an invalid save snapshot.");
            return false;
        }

        if (!m_Context || !m_Context->gameMode)
        {
            LOG_ERROR("[SaveSystem] Cannot restore snapshot: RuntimeContext or GameMode is null.");
            return false;
        }

        m_LastRestoreChecksumValid = true;

        // Verify active scene name matches
        std::string currentScene;
        auto* sceneMgr = dynamic_cast<SceneManager*>(m_Context->scenes);
        if (sceneMgr && sceneMgr->GetActiveScene())
        {
            currentScene = sceneMgr->GetActiveScene()->GetName();
        }

        if (snapshot.SceneName != currentScene)
        {
            m_LastRestoreResult = "Failed (Scene mismatch)";
            LOG_ERROR("[SaveSystem] Scene mismatch on restore! Save scene: '%s', Current scene: '%s'", 
                snapshot.SceneName.c_str(), currentScene.c_str());
            return false;
        }
        m_LastRestoreSceneMatch = true;

        // 1. Restore GameState mutable structures
        auto& gs = m_Context->gameMode->GetGameStateMutable();
        gs.ActiveObjectiveID = snapshot.ActiveObjectiveID;
        gs.CompletedObjectives = snapshot.CompletedObjectives;
        gs.CurrentCheckpointID = snapshot.CheckpointID;
        gs.SessionState = GameSessionState::Playing;

        // 2. Restore Objective System
        auto* objSys = m_Context->gameMode->GetObjectiveSystem();
        if (objSys)
        {
            objSys->RestoreObjectiveState(snapshot.ActiveObjectiveID, snapshot.CompletedObjectives);
        }

        // 3. Restore Player State (Transform, Health, Alive status)
        Entity player = m_Context->gameMode->FindPlayerEntity();
        if (player != INVALID_ENTITY && m_Context->ecs)
        {
            auto& coordinator = m_Context->ecs->getCoordinator();
            auto transformType = coordinator.GetComponentType<TransformComponent>();
            if (coordinator.GetSignature(player).test(transformType))
            {
                coordinator.GetComponent<TransformComponent>(player) = snapshot.PlayerTransform;
            }
            auto pscType = coordinator.GetComponentType<PlayerStateComponent>();
            if (coordinator.GetSignature(player).test(pscType))
            {
                auto& psc = coordinator.GetComponent<PlayerStateComponent>(player);
                psc.Health = snapshot.PlayerHealth;
                psc.IsAlive = snapshot.PlayerAlive;
                psc.CurrentInteractionTarget = INVALID_ENTITY;
            }
        }

        // 4. Restore Simple State Objects & Activatable / Interactable States
        m_LastRestoreObjectsCount = 0;
        m_LastRestoreInteractablesCount = 0;
        m_LastRestoreWarnings = 0;

        if (m_Context->ecs)
        {
            auto& coordinator = m_Context->ecs->getCoordinator();
            auto stateType = coordinator.GetComponentType<SimpleStateComponent>();
            auto activatableType = coordinator.GetComponentType<ActivatableComponent>();
            auto doorType = coordinator.GetComponentType<DoorComponent>();
            auto transformType = coordinator.GetComponentType<TransformComponent>();

            for (Entity ent : coordinator.GetActiveEntities())
            {
                if (ent != INVALID_ENTITY && coordinator.IsEntityAlive(ent))
                {
                    auto sig = coordinator.GetSignature(ent);

                    // Restore Activatable/Interactable flag
                    if (sig.test(activatableType))
                    {
                        auto& act = coordinator.GetComponent<ActivatableComponent>(ent);
                        if (!act.ActivationID.empty())
                        {
                            auto it = snapshot.InteractableActivationStates.find(act.ActivationID);
                            if (it != snapshot.InteractableActivationStates.end())
                            {
                                act.HasActivated = it->second;
                                m_LastRestoreInteractablesCount++;
                            }
                            else
                            {
                                m_LastRestoreWarnings++;
                                LOG_WARN("[SaveSystem] Missing interactable state for ID '%s'", act.ActivationID.c_str());
                            }
                        }
                    }

                    // Restore SimpleStateComponent values
                    if (sig.test(stateType) && sig.test(activatableType))
                    {
                        const auto& act = coordinator.GetComponent<ActivatableComponent>(ent);
                        if (!act.ActivationID.empty())
                        {
                            auto it = snapshot.SimpleObjectStates.find(act.ActivationID);
                            if (it != snapshot.SimpleObjectStates.end())
                            {
                                auto& state = coordinator.GetComponent<SimpleStateComponent>(ent);
                                state.CurrentState = it->second;
                                m_LastRestoreObjectsCount++;

                                // If it is a door, sync transform position
                                if (sig.test(doorType) && sig.test(transformType))
                                {
                                    auto& door = coordinator.GetComponent<DoorComponent>(ent);
                                    auto& transform = coordinator.GetComponent<TransformComponent>(ent);

                                    if (state.CurrentState == SimpleObjectState::Completed)
                                    {
                                        transform.position = door.ClosedPosition + door.OpenOffset;
                                        door.IsOpen = true;
                                        door.IsOpening = false;
                                    }
                                    else
                                    {
                                        transform.position = door.ClosedPosition;
                                        door.IsOpen = false;
                                        door.IsOpening = false;
                                    }
                                }
                            }
                            else
                            {
                                m_LastRestoreWarnings++;
                                LOG_WARN("[SaveSystem] Missing simple state for object ID '%s'", act.ActivationID.c_str());
                            }
                        }
                    }
                }
            }
        }

        m_LastRestoreResult = "Success";
        m_Context->gameMode->ResumeLevel(); // Sync GameSessionState and Resume HUD updates

        LOG_INFO("[SaveSystem] Restored save state successfully. (Restored Objects: %zu, Interactables: %zu, Warnings: %zu)", 
            m_LastRestoreObjectsCount, m_LastRestoreInteractablesCount, m_LastRestoreWarnings);

        return true;
    }

} // namespace eng::runtime
