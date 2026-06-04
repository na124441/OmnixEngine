#pragma once
#include <cstdint>

namespace eng::renderer {

enum class PassID : uint32_t {
    Shadow = 0,
    Geometry,
    Lighting,
    PostProcess,
    UI,
    Count
};

static constexpr uint32_t PASS_COUNT = static_cast<uint32_t>(PassID::Count);

} // namespace eng::renderer
