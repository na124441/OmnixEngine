#pragma once

#include "Serializer/ECS/ECS.h"
#include "Snapshot/ECSSnapshot.h"
#include "Snapshot/SnapshotContext.h"
#include "../../Core/Logger.h"

class SerializationBridge {
private:
    ECSSnapshot m_snapshot;
    const ECS* m_ecsManager;
    const ComponentSchemaRegistry* m_schemaRegistry;

public:
    SerializationBridge(const ECS& ecsManager, const ComponentSchemaRegistry& schemaRegistry)
        : m_ecsManager(&ecsManager), m_schemaRegistry(&schemaRegistry) {}

    [[nodiscard]] const ECSSnapshot& GetSnapshot() const {
        return m_snapshot;
    }

    void Capture(const SnapshotContext& context) {
        if (!m_ecsManager || !m_schemaRegistry) {
            LOG_ERROR("SerializationBridge not initialized.");
            return;
        }
        m_snapshot.Capture(*m_ecsManager, *m_schemaRegistry, context);
    }
};