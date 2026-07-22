#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <cstring>

namespace eng::networking {

    // -------------------------------------------------------------------------
    // 1. Socket Layer & Packet Serialization
    // -------------------------------------------------------------------------
    enum class PacketType : uint8_t {
        Connect = 0,
        Disconnect,
        StateReplication,
        RPC,
        Ping,
        Pong
    };

    struct NetworkPacket {
        PacketType type = PacketType::Connect;
        uint32_t senderId = 0;
        uint32_t sequenceNumber = 0;
        std::vector<uint8_t> payload;

        bool Serialize(std::vector<uint8_t>& outBytes) const {
            outBytes.resize(sizeof(PacketType) + sizeof(uint32_t) * 2 + sizeof(uint32_t) + payload.size());
            uint8_t* ptr = outBytes.data();
            std::memcpy(ptr, &type, sizeof(PacketType)); ptr += sizeof(PacketType);
            std::memcpy(ptr, &senderId, sizeof(uint32_t)); ptr += sizeof(uint32_t);
            std::memcpy(ptr, &sequenceNumber, sizeof(uint32_t)); ptr += sizeof(uint32_t);

            uint32_t payloadSize = static_cast<uint32_t>(payload.size());
            std::memcpy(ptr, &payloadSize, sizeof(uint32_t)); ptr += sizeof(uint32_t);
            if (payloadSize > 0) {
                std::memcpy(ptr, payload.data(), payloadSize);
            }
            return true;
        }

        bool Deserialize(const uint8_t* inBytes, size_t size) {
            if (size < sizeof(PacketType) + sizeof(uint32_t) * 3) return false;
            const uint8_t* ptr = inBytes;
            std::memcpy(&type, ptr, sizeof(PacketType)); ptr += sizeof(PacketType);
            std::memcpy(&senderId, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
            std::memcpy(&sequenceNumber, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);

            uint32_t payloadSize = 0;
            std::memcpy(&payloadSize, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);

            if (payloadSize > 0 && (size - (ptr - inBytes)) >= payloadSize) {
                payload.resize(payloadSize);
                std::memcpy(payload.data(), ptr, payloadSize);
            } else {
                payload.clear();
            }
            return true;
        }
    };

    // -------------------------------------------------------------------------
    // 2. Replication Framework
    // -------------------------------------------------------------------------
    struct EntityNetworkState {
        uint32_t netId = 0;
        uint32_t ownerClientId = 0;
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        uint32_t dirtyMask = 0; // Bitmask for changed properties
    };

    class NetworkReplicator {
    public:
        void RegisterEntity(uint32_t netId, uint32_t ownerId) {
            EntityNetworkState state;
            state.netId = netId;
            state.ownerClientId = ownerId;
            m_Entities[netId] = state;
        }

        void UpdateEntityPosition(uint32_t netId, const glm::vec3& pos) {
            auto it = m_Entities.find(netId);
            if (it != m_Entities.end()) {
                it->second.position = pos;
                it->second.dirtyMask |= (1 << 0);
            }
        }

        const EntityNetworkState* GetEntityState(uint32_t netId) const {
            auto it = m_Entities.find(netId);
            return (it != m_Entities.end()) ? &it->second : nullptr;
        }

        bool BuildReplicationPacket(uint32_t netId, NetworkPacket& outPacket) {
            auto it = m_Entities.find(netId);
            if (it == m_Entities.end() || it->second.dirtyMask == 0) return false;

            outPacket.type = PacketType::StateReplication;
            outPacket.senderId = it->second.ownerClientId;

            outPacket.payload.resize(sizeof(EntityNetworkState));
            std::memcpy(outPacket.payload.data(), &it->second, sizeof(EntityNetworkState));

            it->second.dirtyMask = 0; // Clear dirty mask after building
            return true;
        }

        bool ApplyReplicationPacket(const NetworkPacket& packet) {
            if (packet.type != PacketType::StateReplication || packet.payload.size() < sizeof(EntityNetworkState)) {
                return false;
            }
            EntityNetworkState receivedState;
            std::memcpy(&receivedState, packet.payload.data(), sizeof(EntityNetworkState));
            m_Entities[receivedState.netId] = receivedState;
            return true;
        }

    private:
        std::unordered_map<uint32_t, EntityNetworkState> m_Entities;
    };

    // -------------------------------------------------------------------------
    // 3. Remote Procedure Calls (RPCs)
    // -------------------------------------------------------------------------
    enum class RPCType : uint8_t {
        ServerRPC = 0,
        ClientRPC,
        MulticastRPC
    };

    struct RPCMessage {
        std::string rpcName;
        uint32_t targetNetId = 0;
        std::string paramsJson;
        RPCType rpcType = RPCType::ServerRPC;
    };

    class RPCManager {
    public:
        using RPCHandler = std::function<void(uint32_t netId, const std::string& params)>;

        void RegisterRPC(const std::string& rpcName, RPCHandler handler) {
            m_Handlers[rpcName] = handler;
        }

        bool InvokeRPC(const std::string& rpcName, uint32_t targetNetId, const std::string& paramsJson) {
            auto it = m_Handlers.find(rpcName);
            if (it != m_Handlers.end()) {
                it->second(targetNetId, paramsJson);
                return true;
            }
            return false;
        }

    private:
        std::unordered_map<std::string, RPCHandler> m_Handlers;
    };

    // -------------------------------------------------------------------------
    // 4. Client/Server Loop & Network Driver
    // -------------------------------------------------------------------------
    enum class NetworkRole : uint8_t {
        Standalone = 0,
        DedicatedServer,
        ListenServer,
        Client
    };

    class NetworkDriver {
    public:
        void StartServer(uint16_t port) {
            m_Role = NetworkRole::DedicatedServer;
            m_Port = port;
            m_Connected = true;
        }

        void ConnectClient(const std::string& address, uint16_t port) {
            m_Role = NetworkRole::Client;
            m_Address = address;
            m_Port = port;
            m_Connected = true;
        }

        void Disconnect() {
            m_Connected = false;
            m_Role = NetworkRole::Standalone;
        }

        NetworkRole GetRole() const { return m_Role; }
        bool IsConnected() const { return m_Connected; }
        uint16_t GetPort() const { return m_Port; }

    private:
        NetworkRole m_Role = NetworkRole::Standalone;
        bool m_Connected = false;
        std::string m_Address = "127.0.0.1";
        uint16_t m_Port = 7777;
    };

    // -------------------------------------------------------------------------
    // 5. Client-Side Prediction & Reconciliation
    // -------------------------------------------------------------------------
    struct MoveInputSnapshot {
        uint32_t sequenceNumber = 0;
        glm::vec3 moveInput{ 0.0f, 0.0f, 0.0f };
        float deltaTime = 0.016f;
    };

    class ClientPredictionEngine {
    public:
        void RecordInput(uint32_t seqNum, const glm::vec3& moveInput, float dt) {
            m_PendingInputs.push_back({ seqNum, moveInput, dt });
        }

        glm::vec3 PredictMovement(const glm::vec3& currentPos, const glm::vec3& moveInput, float moveSpeed, float dt) {
            return currentPos + moveInput * moveSpeed * dt;
        }

        glm::vec3 Reconcile(
            const glm::vec3& serverConfirmedPos,
            uint32_t serverConfirmedSeqNum,
            float moveSpeed
        ) {
            // Remove inputs acknowledged by server
            m_PendingInputs.erase(
                std::remove_if(m_PendingInputs.begin(), m_PendingInputs.end(), [&](const MoveInputSnapshot& inp) {
                    return inp.sequenceNumber <= serverConfirmedSeqNum;
                }),
                m_PendingInputs.end()
            );

            // Re-apply unacknowledged inputs on top of server confirmed position
            glm::vec3 reconciledPos = serverConfirmedPos;
            for (const auto& inp : m_PendingInputs) {
                reconciledPos += inp.moveInput * moveSpeed * inp.deltaTime;
            }
            return reconciledPos;
        }

        size_t GetPendingInputCount() const { return m_PendingInputs.size(); }

    private:
        std::vector<MoveInputSnapshot> m_PendingInputs;
    };

    // -------------------------------------------------------------------------
    // 6. Network Time Synchronization
    // -------------------------------------------------------------------------
    class NetworkTimeClock {
    public:
        void ReceivePong(float roundTripTimeMs, float serverTimeSeconds) {
            m_RTTMs = roundTripTimeMs;
            m_ServerTimeOffsetSeconds = serverTimeSeconds + (roundTripTimeMs * 0.0005f); // Half RTT offset
        }

        float GetRTTMs() const { return m_RTTMs; }
        float GetSynchronizedServerTime() const { return m_ServerTimeOffsetSeconds; }

    private:
        float m_RTTMs = 0.0f;
        float m_ServerTimeOffsetSeconds = 0.0f;
    };

} // namespace eng::networking
