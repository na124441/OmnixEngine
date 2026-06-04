#pragma once
#include <string>
#include <vector>

namespace eng::runtime {
    void RegisterSystemStartup(const std::string& name);
    void RegisterSystemShutdown(const std::string& name);
    bool ValidateExecutionSequence();
}
