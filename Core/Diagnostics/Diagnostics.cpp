#include "Core/Diagnostics/Diagnostics.h"
#include "Core/Logging/Logger.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <iomanip>
#include <sstream>

namespace eng::diagnostics {

    static std::unordered_map<std::string, std::string> s_SubsystemHealth;
    static std::recursive_mutex s_DiagnosticsMutex;

    void ReportSubsystemHealth(const std::string& subsystem, const std::string& status) {
        std::lock_guard<std::recursive_mutex> lock(s_DiagnosticsMutex);
        s_SubsystemHealth[subsystem] = status;
        LOG_INFO("[Diagnostics] Subsystem '%s' health update: %s", subsystem.c_str(), status.c_str());
    }

    void PrintDiagnosticsReport() {
        std::lock_guard<std::recursive_mutex> lock(s_DiagnosticsMutex);
        LOG_INFO("=== OMNIX ENGINE DIAGNOSTICS REPORT ===");
        for (const auto& [sys, status] : s_SubsystemHealth) {
            LOG_INFO(" - Subsystem: %-12s | Status/Metrics: %s", sys.c_str(), status.c_str());
        }
        LOG_INFO("=======================================");
    }

} // namespace eng::diagnostics
