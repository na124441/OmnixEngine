#include "Serializer/Serialization/Delta/DeltaSerializer.h"
#include <stdexcept>

DeltaSerializer::DeltaSerializer(BinaryWriter& writer)
    : m_writer(writer)
{
}

// 1. Begin_Frame(frame)
void DeltaSerializer::BeginFrame(uint32_t frameID)
{
    // writer.write(FRAME_BEGIN)
    m_writer.WriteU8(static_cast<uint8_t>(DeltaFrameTag::FrameBegin));

    // writer.write(frame)
    m_writer.WriteU32(frameID);
}

// 2. Serialize_Entity_Delta(delta)
void DeltaSerializer::SerializeEntityDelta(const EntityDeltaEvent& delta)
{
    // writer.write(ENTITY_DELTA_TAG)
    m_writer.WriteU8(static_cast<uint8_t>(DeltaFrameTag::EntityDeltaTag));

    // writer.write(delta.entity)
    m_writer.WriteU64(delta.entityID);

    // writer.write(delta.operation)
    m_writer.WriteU8(static_cast<uint8_t>(delta.type));
}

// 3. Serialize_Component_Delta(delta)
void DeltaSerializer::SerializeComponentDelta(const ComponentDeltaEvent& delta)
{
    // writer.write(COMPONENT_DELTA_TAG)
    m_writer.WriteU8(static_cast<uint8_t>(DeltaFrameTag::ComponentDeltaTag));

    // writer.write(delta.entity)
    m_writer.WriteU64(delta.entityID);

    // writer.write(delta.operation)
    m_writer.WriteU8(static_cast<uint8_t>(delta.type));

    // if delta.operation == COMPONENT_ADDED or delta.operation == COMPONENT_MODIFIED:
    if (delta.type == ComponentDeltaType::Added || delta.type == ComponentDeltaType::Modified)
    {
        // writer.write(delta.size)
        m_writer.WriteU32(static_cast<uint32_t>(delta.dataSize));

        // writer.writeBytes(delta.data, delta.size)
        // Note: Component data would be written externally after this call
        // This method only writes the metadata
    }
}

// 4. End_Frame()
void DeltaSerializer::EndFrame()
{
    // writer.write(FRAME_END)
    m_writer.WriteU8(static_cast<uint8_t>(DeltaFrameTag::FrameEnd));
}

// Serialize entire delta frame in sequence
void DeltaSerializer::SerializeDeltaFrame(const DeltaFrame& deltaFrame)
{
    BeginFrame(deltaFrame.frameID);

    // Serialize all entity deltas
    for (const EntityDeltaEvent& entityDelta : deltaFrame.entityDeltas)
    {
        SerializeEntityDelta(entityDelta);
    }

    // Serialize all component deltas
    for (const ComponentDeltaEvent& componentDelta : deltaFrame.componentDeltas)
    {
        SerializeComponentDelta(componentDelta);
    }

    EndFrame();
}

// Serialize snapshot-based delta
void DeltaSerializer::SerializeSnapshotDelta(const DeltaSnapshot& snapshot)
{
    // Write delta header information
    m_writer.WriteU8(static_cast<uint8_t>(DeltaFrameTag::FrameBegin));
    m_writer.WriteU64(snapshot.baseSnapshotID);
    m_writer.WriteU64(snapshot.targetSnapshotID);
    m_writer.WriteU32(static_cast<uint32_t>(snapshot.GetEntityDeltaCount()));

    // Serialize each entity delta
    for (const EntityDelta& entityDelta : snapshot.entityDeltas)
    {
        // Write entity ID and operation
        m_writer.WriteU64(entityDelta.entityID);
        m_writer.WriteU8(static_cast<uint8_t>(entityDelta.operation));

        // Write component delta count
        m_writer.WriteU32(static_cast<uint32_t>(entityDelta.GetComponentDeltaCount()));

        // Serialize each component delta
        for (const ComponentDelta& componentDelta : entityDelta.componentDeltas)
        {
            // Write component type ID and operation
            m_writer.WriteU32(componentDelta.componentTypeID);
            m_writer.WriteU8(static_cast<uint8_t>(componentDelta.operation));

            // Write field delta count
            m_writer.WriteU32(static_cast<uint32_t>(componentDelta.GetFieldDeltaCount()));

            // Serialize each field delta
            for (const FieldDelta& fieldDelta : componentDelta.fieldDeltas)
            {
                // Write field ID and change type
                m_writer.WriteU32(fieldDelta.fieldID);
                m_writer.WriteU8(static_cast<uint8_t>(fieldDelta.changeType));

                // Write old value
                m_writer.WriteString(fieldDelta.oldValue);

                // Write new value
                m_writer.WriteString(fieldDelta.newValue);
            }
        }
    }

    m_writer.WriteU8(static_cast<uint8_t>(DeltaFrameTag::FrameEnd));
}