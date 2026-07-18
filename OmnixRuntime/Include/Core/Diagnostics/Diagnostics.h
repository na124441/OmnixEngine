#pragma once
#include <string>

namespace eng::diagnostics {
    void ReportSubsystemHealth(const std::string& subsystem, const std::string& status);
    void PrintDiagnosticsReport();
}
