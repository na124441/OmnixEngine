#pragma once
#include <cstdint>

namespace eng::core {

    enum class ResultCode : uint16_t {
        Success = 0,
        Failure,
        InvalidArgument,
        OutOfMemory,
        NotInitialized,
    };

} // namespace eng::core
