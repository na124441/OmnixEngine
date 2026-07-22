#include <stdexcept>
#include <functional>
#include <algorithm>
#include "Core/Serialization/Delta/DeltaTracker.h"

DeltaTracker::DeltaTracker()
    : m_frameInProgress(false)
{
}

// Hash function for component data using FNV-1a algorithm
ComponentHash DeltaTracker::ComputeHash(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0)
    {
        return 0;
    }

    // FNV-1a constants
    constexpr ComponentHash FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr ComponentHash FNV_PRIME = 1099511628211ULL;

    ComponentHash hash = FNV_OFFSET_BASIS;

    for (size_t i = 0; i < size; ++i)
    {
        hash ^= static_cast<ComponentHash>(data[i]);
        hash *= FNV_PRIME;
    }

    return hash;
}

// 1. BeginFrame(frame_id)
void DeltaTracker::BeginFrame(uint32_t frameID)
{
    if (m_frameInProgress)
    {
        throw std::runtime_error("Cannot begin frame: frame already in progress. Call EndFrame() first.");
    }

    // delta_frame.clear()
    m_deltaFrame.Clear();

    // delta_frame.frame = frame_id
    m_deltaFrame.frameID = frameID;

    // current_state.clear()
    m_currentState.clear();

    m_frameInProgress = true;
}

// 2. On_Entity_Created(entity)
void DeltaTracker::OnEntityCreated(uint64_t entityID)
{
    if (!m_frameInProgress)
    {
        throw std::runtime_error("Cannot track entity creation: no frame in progress. Call BeginFrame() first.");
    }

    // delta_frame.entity_deltas.push(entity, ENTITY_CREATED)
    EntityDeltaEvent event(entityID, EntityDeltaType::Created);
    m_deltaFrame.entityDeltas.push_back(event);
}

// 3. On_Entity_Destroyed(entity)
void DeltaTracker::OnEntityDestroyed(uint64_t entityID)
{
    if (!m_frameInProgress)
    {
        throw std::runtime_error("Cannot track entity destruction: no frame in progress. Call BeginFrame() first.");
    }

    // delta_frame.entity_deltas.push(entity, ENTITY_DESTROYED)
    EntityDeltaEvent event(entityID, EntityDeltaType::Destroyed);
    m_deltaFrame.entityDeltas.push_back(event);
}

// 4. Track_Component(entity, component, data, size)
void DeltaTracker::TrackComponent(
    uint64_t entityID,
    uint32_t componentTypeID,
    const uint8_t* data,
    size_t size)
{
    if (!m_frameInProgress)
    {
        throw std::runtime_error("Cannot track component: no frame in progress. Call BeginFrame() first.");
    }

    // hash = HASH(data, size)
    ComponentHash currentHash = ComputeHash(data, size);

    // current_state[entity][component] = hash
    m_currentState[entityID][componentTypeID] = currentHash;

    // if entity not in previous_state:
    //   emit COMPONENT_ADDED
    //   return
    auto prevEntityIt = m_previousState.find(entityID);
    if (prevEntityIt == m_previousState.end())
    {
        ComponentDeltaEvent event(entityID, componentTypeID, ComponentDeltaType::Added, size);
        m_deltaFrame.componentDeltas.push_back(event);
        return;
    }

    // if component not in previous_state[entity]:
    //   emit COMPONENT_ADDED
    auto& prevComponentState = prevEntityIt->second;
    auto prevComponentIt = prevComponentState.find(componentTypeID);
    if (prevComponentIt == prevComponentState.end())
    {
        ComponentDeltaEvent event(entityID, componentTypeID, ComponentDeltaType::Added, size);
        m_deltaFrame.componentDeltas.push_back(event);
        return;
    }

    // if previous_state[entity][component] != hash:
    //   emit COMPONENT_MODIFIED
    if (prevComponentIt->second != currentHash)
    {
        ComponentDeltaEvent event(entityID, componentTypeID, ComponentDeltaType::Modified, size);
        m_deltaFrame.componentDeltas.push_back(event);
    }
}

// 5. End_Frame()
DeltaFrame DeltaTracker::EndFrame()
{
    if (!m_frameInProgress)
    {
        throw std::runtime_error("Cannot end frame: no frame in progress. Call BeginFrame() first.");
    }

    // for each entity in previous_state:
    //   if entity not in current_state:
    //     continue
    //   for each component in previous_state[entity]:
    //     if component not in current_state[entity]:
    //       emit COMPONENT_REMOVED

    for (const auto& [entityID, prevComponentMap] : m_previousState)
    {
        // Skip if entity was destroyed (already handled by OnEntityDestroyed)
        auto currentEntityIt = m_currentState.find(entityID);
        if (currentEntityIt == m_currentState.end())
        {
            continue;
        }

        const auto& currentComponentMap = currentEntityIt->second;

        for (const auto& [componentTypeID, prevHash] : prevComponentMap)
        {
            // if component not in current_state[entity]:
            //   emit COMPONENT_REMOVED
            auto currentComponentIt = currentComponentMap.find(componentTypeID);
            if (currentComponentIt == currentComponentMap.end())
            {
                ComponentDeltaEvent event(entityID, componentTypeID, ComponentDeltaType::Removed, 0);
                m_deltaFrame.componentDeltas.push_back(event);
            }
        }
    }

    // Save current state as previous state for next frame
    m_previousState = m_currentState;

    m_frameInProgress = false;

    return m_deltaFrame;
}

void DeltaTracker::Reset()
{
    m_deltaFrame.Clear();
    m_currentState.clear();
    m_previousState.clear();
    m_frameInProgress = false;
}
