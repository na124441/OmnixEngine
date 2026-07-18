#pragma once

#include "Gameplay/GameMode.h"

namespace eng::runtime {

    class VerticalSliceGameMode : public GameMode
    {
    public:
        void OnLevelStart(RuntimeContext* context) override;
        void OnLevelEnd() override;

        void Tick(float dt) override;
    };

} // namespace eng::runtime
