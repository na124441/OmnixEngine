#include "Core/Serialization/Delta/DeltaSnapshot.h"
#include "Serializer/ECS/Snapshot/ComponentSnapshot.h"
#include "Serializer/ECS/Snapshot/FieldSnapshot.h"
#include <algorithm>

// Helper function to find an entity in a vector of snapshots
static const EntitySnapshot* FindEntity(const std::vector<EntitySnapshot>& entities, uint64_t entityID) {
    auto it = std::find_if(entities.begin(), entities.end(),
        [entityID](const EntitySnapshot& e) { return e.GetEntityID() == entityID; });
    return it != entities.end() ? &(*it) : nullptr;
}

// Helper function to find a component in an entity snapshot
static const ComponentSnapshot* FindComponent(const EntitySnapshot& entity, uint32_t componentTypeID) {
    return entity.GetComponentSnapshot(componentTypeID);
}

// Helper function to find a field in a component snapshot
static const FieldSnapshot* FindField(const ComponentSnapshot& component, uint32_t fieldID) {
    const auto& fields = component.GetFieldSnapshots();
    auto it = std::find_if(fields.begin(), fields.end(),
        [fieldID](const FieldSnapshot& f) { return f.GetFieldID() == fieldID; });
    return it != fields.end() ? &(*it) : nullptr;
}

static void DiffFields(const ComponentSnapshot& componentA, const ComponentSnapshot& componentB, ComponentDelta& componentDelta) {
    for (const auto& fieldB : componentB.GetFieldSnapshots()) {
        const FieldSnapshot* fieldA = FindField(componentA, fieldB.GetFieldID());
        if (!fieldA) {
            componentDelta.fieldDeltas.push_back({ fieldB.GetFieldID(), FieldChangeType::Set, "", FieldDataToString(fieldB) });
        } else if (!fieldA->GetFieldData().Equals(fieldB.GetFieldData())) {
            componentDelta.fieldDeltas.push_back({ fieldB.GetFieldID(), FieldChangeType::Set, FieldDataToString(*fieldA), FieldDataToString(fieldB) });
        }
    }
}

static void DiffComponents(const EntitySnapshot& entityA, const EntitySnapshot& entityB, EntityDelta& entityDelta) {
    for (const auto& pairB : entityB.GetComponentSnapshots()) {
        const auto& componentB = pairB.second;
        const ComponentSnapshot* componentA = FindComponent(entityA, componentB.GetComponentTypeID());
        if (!componentA) {
            ComponentDelta componentDelta{componentB.GetComponentTypeID(), ComponentOperation::Add};
            for (const auto& field : componentB.GetFieldSnapshots()) {
                componentDelta.fieldDeltas.push_back({ field.GetFieldID(), FieldChangeType::Set, "", FieldDataToString(field) });
            }
            entityDelta.componentDeltas.push_back(componentDelta);
        } else {
            ComponentDelta componentDelta{componentB.GetComponentTypeID(), ComponentOperation::Modify};
            DiffFields(*componentA, componentB, componentDelta);
            if (!componentDelta.fieldDeltas.empty()) {
                entityDelta.componentDeltas.push_back(componentDelta);
            }
        }
    }
    for (const auto& pairA : entityA.GetComponentSnapshots()) {
        if (!FindComponent(entityB, pairA.first)) {
            entityDelta.componentDeltas.push_back({pairA.first, ComponentOperation::Remove});
        }
    }
}

DeltaSnapshot DeltaSnapshot_Builder::BuildDelta(
    uint64_t baseSnapshotID,
    uint64_t targetSnapshotID,
    const std::vector<EntitySnapshot>& snapshotA,
    const std::vector<EntitySnapshot>& snapshotB)
{
    DeltaSnapshot delta{baseSnapshotID, targetSnapshotID};
    for (const auto& entityA : snapshotA) {
        const EntitySnapshot* entityB = FindEntity(snapshotB, entityA.GetEntityID());
        if (!entityB) {
            delta.entityDeltas.push_back({entityA.GetEntityID(), EntityOperation::Destroy});
        } else {
            EntityDelta entityDelta{entityA.GetEntityID(), EntityOperation::Modify};
            DiffComponents(entityA, *entityB, entityDelta);
            if (!entityDelta.componentDeltas.empty()) {
                delta.entityDeltas.push_back(entityDelta);
            }
        }
    }
    for (const auto& entityB : snapshotB) {
        if (!FindEntity(snapshotA, entityB.GetEntityID())) {
            EntityDelta entityDelta{entityB.GetEntityID(), EntityOperation::Create};
            for (const auto& pair : entityB.GetComponentSnapshots()) {
                ComponentDelta componentDelta{pair.first, ComponentOperation::Add};
                for (const auto& field : pair.second.GetFieldSnapshots()) {
                    componentDelta.fieldDeltas.push_back({field.GetFieldID(), FieldChangeType::Set, "", FieldDataToString(field)});
                }
                entityDelta.componentDeltas.push_back(componentDelta);
            }
            delta.entityDeltas.push_back(entityDelta);
        }
    }
    return delta;
}

static void ApplyFieldOperation(ComponentSnapshot& component, const FieldDelta& fieldDelta) {
    // This is complex and requires schema information to properly deserialize the string data.
    // For now, this is a placeholder.
}

static void ApplyComponentOperation(EntitySnapshot& entity, const ComponentDelta& componentDelta) {
    auto& components = entity.GetComponentSnapshots();
    switch (componentDelta.operation) {
        case ComponentOperation::Add: {
            ComponentSnapshot newComponent(componentDelta.componentTypeID);
            for (const auto& fieldDelta : componentDelta.fieldDeltas) {
                ApplyFieldOperation(newComponent, fieldDelta);
            }
            components[componentDelta.componentTypeID] = newComponent;
            break;
        }
        case ComponentOperation::Remove:
            components.erase(componentDelta.componentTypeID);
            break;
        case ComponentOperation::Modify: {
            auto it = components.find(componentDelta.componentTypeID);
            if (it != components.end()) {
                for (const auto& fieldDelta : componentDelta.fieldDeltas) {
                    ApplyFieldOperation(it->second, fieldDelta);
                }
            }
            break;
        }
    }
}

static void ApplyEntityOperation(std::vector<EntitySnapshot>& snapshot, const EntityDelta& entityDelta) {
    switch (entityDelta.operation) {
        case EntityOperation::Create: {
            EntitySnapshot newEntity(entityDelta.entityID);
            for (const auto& compDelta : entityDelta.componentDeltas) {
                ApplyComponentOperation(newEntity, compDelta);
            }
            snapshot.push_back(newEntity);
            break;
        }
        case EntityOperation::Destroy:
            snapshot.erase(std::remove_if(snapshot.begin(), snapshot.end(),
                [&](const EntitySnapshot& e) { return e.GetEntityID() == entityDelta.entityID; }),
                snapshot.end());
            break;
        case EntityOperation::Modify: {
            auto it = std::find_if(snapshot.begin(), snapshot.end(),
                [&](const EntitySnapshot& e) { return e.GetEntityID() == entityDelta.entityID; });
            if (it != snapshot.end()) {
                for (const auto& compDelta : entityDelta.componentDeltas) {
                    ApplyComponentOperation(*it, compDelta);
                }
            }
            break;
        }
    }
}

std::vector<EntitySnapshot> DeltaSnapshot_Applier::ApplyDelta(
    const std::vector<EntitySnapshot>& baseSnapshot,
    const DeltaSnapshot& delta)
{
    std::vector<EntitySnapshot> newSnapshot = baseSnapshot;
    for (const auto& entityDelta : delta.entityDeltas) {
        ApplyEntityOperation(newSnapshot, entityDelta);
    }
    return newSnapshot;
}
