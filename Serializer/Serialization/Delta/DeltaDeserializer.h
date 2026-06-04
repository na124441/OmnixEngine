#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "DeltaTracker.h"
#include "DeltaSnapshot.h"
#include "../Normal/Binary/BinaryReader.h"

class DeltaDeserializer
{
private:
    BinaryReader& m_reader;

    // Helper methods
    DeltaFrameTag ReadTag();
    EntityDeltaEvent DeserializeEntityDelta();
    ComponentDeltaEvent DeserializeComponentDelta();

public:
    DeltaDeserializer(BinaryReader& reader);
    ~DeltaDeserializer() = default;

    // Main deserialization API
    DeltaFrame DeserializeDeltaFrame();
    DeltaSnapshot DeserializeSnapshotDelta();
};