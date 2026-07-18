#include "Gameplay/VerticalSliceGameMode.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    void VerticalSliceGameMode::OnLevelStart(RuntimeContext* context) {
        GameMode::OnLevelStart(context);
        LOG_INFO("Vertical Slice Started");
    }

    void VerticalSliceGameMode::OnLevelEnd() {
        GameMode::OnLevelEnd();
        LOG_INFO("Vertical Slice Ended");
    }

    void VerticalSliceGameMode::Tick(float dt) {
        GameMode::Tick(dt);
        LOG_INFO("Vertical Slice Tick");
    }

} // namespace eng::runtime
