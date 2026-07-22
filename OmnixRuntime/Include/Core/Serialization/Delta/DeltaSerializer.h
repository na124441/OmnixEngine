#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "Core/Serialization/Delta/DeltaTracker.h"
#include "Core/Serialization/Delta/DeltaSnapshot.h"
#include "../Normal/Binary/BinaryWriter.h"

class DeltaSerializer
{
private:
    BinaryWriter& m_writer;

public:
    DeltaSerializer(BinaryWriter& writer);
    ~DeltaSerializer() = default;

    // Main serialization API
    void BeginFrame(uint32_t frameID);
    void SerializeEntityDelta(const EntityDeltaEvent& delta);
    void SerializeComponentDelta(const ComponentDeltaEvent& delta);
    void EndFrame();

    // Serialize entire delta frame
    void SerializeDeltaFrame(const DeltaFrame& deltaFrame);

    // Serialize snapshot-based deltas
    void SerializeSnapshotDelta(const DeltaSnapshot& snapshot);
};
