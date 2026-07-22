#pragma once

#include <string>
#include <vector>

namespace eng::core {

    struct ExecutionPlan {
        std::vector<std::string> topologicalOrder;
        bool isValid = false;
    };

} // namespace eng::core
