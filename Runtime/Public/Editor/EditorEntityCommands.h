#pragma once

#include "ECSconfig.h"
#include "ECS/Coordinator.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "Runtime/Public/Editor/EditorSelection.h"

namespace eng::runtime {

    class EditorEntityCommands {
    public:
        static Entity CreateEmpty(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection);
        static Entity CreatePlayerStart(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection);
        static Entity CreateDirectionalLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection);
        static Entity CreatePointLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection);
        static Entity CreateSkyLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection);
        static Entity CreateSpotLight(Coordinator& coordinator, EditorDirtyState& dirtyState, EditorSelection& selection);
        static void Delete(Coordinator& coordinator, Entity entity, EditorDirtyState& dirtyState, EditorSelection& selection);
        static Entity Duplicate(Coordinator& coordinator, Entity source, EditorDirtyState& dirtyState, EditorSelection& selection);
    };

} // namespace eng::runtime
