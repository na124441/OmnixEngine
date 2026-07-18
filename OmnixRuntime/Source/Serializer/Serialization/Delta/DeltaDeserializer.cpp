#include "Serializer/Serialization/Delta/DeltaDeserializer.h"
#include "Serializer/Serialization/Delta/DeltaTracker.h"
#include <stdexcept>
#include <string>

DeltaDeserializer::DeltaDeserializer(BinaryReader& reader)
    : m_reader(reader)
{
}

DeltaFrameTag DeltaDeserializer::ReadTag()
{
    uint8_t tagByte = m_reader.ReadU8();
    return static_cast<DeltaFrameTag>(tagByte);
}

EntityDeltaEvent DeltaDeserializer::DeserializeEntityDelta()
{
    // Read entity ID
    uint64_t entityID = m_reader.ReadU64();

    // Read operation type
    uint8_t operationType = m_reader.ReadU8();
    EntityDeltaType type = static_cast<EntityDeltaType>(operationType);

    return EntityDeltaEvent(entityID, type);
}

ComponentDeltaEvent DeltaDeserializer::DeserializeComponentDelta()
{
    // Read entity ID
    uint64_t entityID = m_reader.ReadU64();

    // Read operation type
    uint8_t operationType = m_reader.ReadU8();
    ComponentDeltaType type = static_cast<ComponentDeltaType>(operationType);

    // Read data size if operation is ADD or MODIFY
    size_t dataSize = 0;
    if (type == ComponentDeltaType::Added || type == ComponentDeltaType::Modified)
    {
        dataSize = m_reader.ReadU32();
    }

    return ComponentDeltaEvent(entityID, 0, type, dataSize);  // componentTypeID would be in the data
}

DeltaFrame DeltaDeserializer::DeserializeDeltaFrame()
{
    // Read FRAME_BEGIN tag
    DeltaFrameTag tag = ReadTag();
    if (tag != DeltaFrameTag::FrameBegin)
    {
        throw std::runtime_error("Expected FRAME_BEGIN tag, got: " + std::to_string(static_cast<uint8_t>(tag)));
    }

    // Read frame ID
    uint32_t frameID = m_reader.ReadU32();
    DeltaFrame deltaFrame(frameID);

    // Read deltas until FRAME_END
    while (true)
    {
        tag = ReadTag();

        if (tag == DeltaFrameTag::FrameEnd)
        {
            break;
        }
        else if (tag == DeltaFrameTag::EntityDeltaTag)
        {
            EntityDeltaEvent entityDelta = DeserializeEntityDelta();
            deltaFrame.entityDeltas.push_back(entityDelta);
        }
        else if (tag == DeltaFrameTag::ComponentDeltaTag)
        {
            ComponentDeltaEvent componentDelta = DeserializeComponentDelta();
            deltaFrame.componentDeltas.push_back(componentDelta);
        }
        else
        {
            throw std::runtime_error("Unknown delta tag: " + std::to_string(static_cast<uint8_t>(tag)));
        }
    }

    return deltaFrame;
}

DeltaSnapshot DeltaDeserializer::DeserializeSnapshotDelta()
{
    // Read FRAME_BEGIN tag
    DeltaFrameTag tag = ReadTag();
    if (tag != DeltaFrameTag::FrameBegin)
    {
        throw std::runtime_error("Expected FRAME_BEGIN tag for snapshot delta");
    }

    // Read snapshot IDs
    uint64_t baseSnapshotID = m_reader.ReadU64();
    uint64_t targetSnapshotID = m_reader.ReadU64();

    DeltaSnapshot snapshot(baseSnapshotID, targetSnapshotID);

    // Read entity delta count
    uint32_t entityDeltaCount = m_reader.ReadU32();

    // Deserialize entity deltas
    for (uint32_t i = 0; i < entityDeltaCount; ++i)
    {
        uint64_t entityID = m_reader.ReadU64();
        uint8_t opByte = m_reader.ReadU8();
        EntityOperation entityOp = static_cast<EntityOperation>(opByte);

        EntityDelta entityDelta(entityID, entityOp);

        // Read component delta count
        uint32_t componentDeltaCount = m_reader.ReadU32();

        // Deserialize component deltas
        for (uint32_t j = 0; j < componentDeltaCount; ++j)
        {
            uint32_t componentTypeID = m_reader.ReadU32();
            uint8_t compOpByte = m_reader.ReadU8();
            ComponentOperation componentOp = static_cast<ComponentOperation>(compOpByte);

            ComponentDelta componentDelta(componentTypeID, componentOp);

            // Read field delta count
            uint32_t fieldDeltaCount = m_reader.ReadU32();

            // Deserialize field deltas
            for (uint32_t k = 0; k < fieldDeltaCount; ++k)
            {
                uint32_t fieldID = m_reader.ReadU32();
                uint8_t changeByte = m_reader.ReadU8();
                FieldChangeType changeType = static_cast<FieldChangeType>(changeByte);

                // Read old value
                std::string oldValue = m_reader.ReadString();

                // Read new value
                std::string newValue = m_reader.ReadString();

                FieldDelta fieldDelta(fieldID, changeType, oldValue, newValue);
                componentDelta.fieldDeltas.push_back(fieldDelta);
            }

            entityDelta.componentDeltas.push_back(componentDelta);
        }

        snapshot.entityDeltas.push_back(entityDelta);
    }

    // Read FRAME_END tag
    tag = ReadTag();
    if (tag != DeltaFrameTag::FrameEnd)
    {
        throw std::runtime_error("Expected FRAME_END tag");
    }

    return snapshot;
}