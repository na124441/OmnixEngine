#pragma once

#include "Gameplay/Components/InteractableComponent.h"

#ifndef OMNIX_ENTITY_DEFINED
#define OMNIX_ENTITY_DEFINED
using Entity = std::uint32_t;
constexpr Entity INVALID_ENTITY = 0;
#endif

namespace eng::runtime {

    struct InteractionPromptData
    {
        bool Visible = false;
        std::string Text;
        InteractionType Type = InteractionType::None;
        Entity Target = INVALID_ENTITY;
    };

} // namespace eng::runtime
