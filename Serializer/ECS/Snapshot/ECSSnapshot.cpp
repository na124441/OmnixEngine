#include "ECSSnapshot.h"
#include "../ECS.h"
#include "../SchemaRegistry.h"
#include "SnapshotContext.h"
#include "../../Serialization/Normal/Binary/BinaryWriter.h"
#include "../../Serialization/Normal/Binary/BinaryReader.h"

ECSSnapshot::ECSSnapshot()
    : m_SnapshotID(0)
{
}

ECSSnapshot::ECSSnapshot(uint64_t snapshotID)
    : m_SnapshotID(snapshotID)
{
}

void ECSSnapshot::Capture(const ECS& ecs, const ComponentSchemaRegistry& registry, const SnapshotContext& context)
{
    m_EntitySnapshots.clear();
    const auto& entities = ecs.GetAllEntities();
    for (uint32_t entityID : entities)
    {
        const auto* entity = ecs.GetEntityMetadata(entityID);
        if (entity && entity->state == ENTITY_ALIVE)
        {
            EntitySnapshot entitySnapshot(entityID, entity->generation);
            if (entitySnapshot.CaptureEntity(ecs, registry, context))
            {
                m_EntitySnapshots.push_back(entitySnapshot);
            }
        }
    }
}

uint64_t ECSSnapshot::GetSnapshotID() const
{
    return m_SnapshotID;
}

const std::vector<EntitySnapshot>& ECSSnapshot::GetEntitySnapshots() const
{
    return m_EntitySnapshots;
}

std::vector<EntitySnapshot>& ECSSnapshot::GetEntitySnapshots()
{
    return m_EntitySnapshots;
}

void ECSSnapshot::Serialize(BinaryWriter& writer) const
{
    writer.WriteU64(m_SnapshotID);
    writer.WriteU32(static_cast<uint32_t>(m_EntitySnapshots.size()));
    for (const auto& entitySnapshot : m_EntitySnapshots)
    {
        entitySnapshot.Serialize(writer);
    }
}

void ECSSnapshot::Deserialize(BinaryReader& reader, const ComponentSchemaRegistry& registry)
{
    m_SnapshotID = reader.ReadU64();
    uint32_t entityCount = reader.ReadU32();
    m_EntitySnapshots.resize(entityCount);
    for (uint32_t i = 0; i < entityCount; ++i)
    {
        m_EntitySnapshots[i].Deserialize(reader, registry);
    }
}