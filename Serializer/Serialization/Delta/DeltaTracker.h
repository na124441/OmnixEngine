//It keeps a previous state hash/shadow copy of components and compares it with the current ECS state
// 1 .BeginFrame(frame_id)
//		delta_frame.clear()
//		detla_frame.frame = frame_id
//		current_state.clear()
// 2.On_Entity_Created(entity):
//		delta_frame.entity_deltas.push(entity, ENTITY_CREATED)
// 3.On_Entity_Destroyed(entity):
//		delta_frame.entity_deltas.push(entity, ENTITY_DESTROYED)
// 4.Track_Component(entity , component , data , size):
//		hash = HASH(data , size)
//		current_state[entity][component] = hash
//		if entity not in previous_state:
//			emity COMPONENT_ADDED
//			return
//		if component not in previous_state[entity]:
//			emit COMPONENT_ADDED
//		if previous_state[entity][component] != hash : 
//			emit COMPONENT_MODIFIED
// 5. End_Frame():
//		for each entity in previous_state:
//			if entity not in current_state:
//				continue
//			for each component in previous_state[entity]:
//				if component  not in current_state[entity]:
//					emit COMPONENT_REMOVED
//	
//
#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <functional>

// Component delta types
enum class ComponentDeltaType : uint8_t
{
    Added = 0,
    Removed = 1,
    Modified = 2
};

// Entity delta types
enum class EntityDeltaType : uint8_t
{
    Created = 0,
    Destroyed = 1
};

// Delta frame tags
enum class DeltaFrameTag : uint8_t
{
    FrameBegin = 0xFB,
    FrameEnd = 0xFE,
    EntityDeltaTag = 0xED,
    ComponentDeltaTag = 0xCD
};

// Component-level delta event
struct ComponentDeltaEvent
{
    uint64_t entityID;
    uint32_t componentTypeID;
    ComponentDeltaType type;
    size_t dataSize;

    ComponentDeltaEvent() : entityID(0), componentTypeID(0), type(ComponentDeltaType::Added), dataSize(0) {}
    ComponentDeltaEvent(uint64_t eID, uint32_t cTypeID, ComponentDeltaType t, size_t size)
        : entityID(eID), componentTypeID(cTypeID), type(t), dataSize(size) {
    }
};

// Entity-level delta event
struct EntityDeltaEvent
{
    uint64_t entityID;
    EntityDeltaType type;

    EntityDeltaEvent() : entityID(0), type(EntityDeltaType::Created) {}
    EntityDeltaEvent(uint64_t eID, EntityDeltaType t) : entityID(eID), type(t) {}
};

// Frame delta container
struct DeltaFrame
{
    uint32_t frameID;
    std::vector<EntityDeltaEvent> entityDeltas;
    std::vector<ComponentDeltaEvent> componentDeltas;

    DeltaFrame() : frameID(0) {}
    explicit DeltaFrame(uint32_t fID) : frameID(fID) {}

    void Clear()
    {
        entityDeltas.clear();
        componentDeltas.clear();
    }

    size_t GetTotalDeltaCount() const
    {
        return entityDeltas.size() + componentDeltas.size();
    }
};

// Hash type for component state
using ComponentHash = uint64_t;

// State tracking structures
// State: Map<EntityID, Map<ComponentTypeID, ComponentHash>>
using ComponentStateMap = std::unordered_map<uint32_t, ComponentHash>;
using EntityStateMap = std::unordered_map<uint64_t, ComponentStateMap>;

class DeltaTracker
{
private:
    DeltaFrame m_deltaFrame;
    EntityStateMap m_currentState;
    EntityStateMap m_previousState;
    bool m_frameInProgress = false;

    // Hash function for component data
    static ComponentHash ComputeHash(const uint8_t* data, size_t size);

public:
    DeltaTracker();
    ~DeltaTracker() = default;

    // Main tracking API
    void BeginFrame(uint32_t frameID);
    void OnEntityCreated(uint64_t entityID);
    void OnEntityDestroyed(uint64_t entityID);
    void TrackComponent(uint64_t entityID, uint32_t componentTypeID, const uint8_t* data, size_t size);
    DeltaFrame EndFrame();

    // Utility methods
    bool IsFrameInProgress() const { return m_frameInProgress; }
    const DeltaFrame& GetCurrentDeltaFrame() const { return m_deltaFrame; }
    const EntityStateMap& GetCurrentState() const { return m_currentState; }
    const EntityStateMap& GetPreviousState() const { return m_previousState; }

    // Reset tracking state
    void Reset();
};