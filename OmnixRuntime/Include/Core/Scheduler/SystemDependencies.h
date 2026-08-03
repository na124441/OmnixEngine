#pragma once

#include <string>
#include <vector>

namespace eng::core {

    struct SystemDependencies {
        std::vector<std::string> reads;
        std::vector<std::string> writes;
    };

} // namespace eng::core
