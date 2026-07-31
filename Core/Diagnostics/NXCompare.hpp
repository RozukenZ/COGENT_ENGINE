#pragma once
#include "TelemetrySystem.hpp"
#include <string>
#include <vector>

namespace Cogent::Core::Diagnostics {

    enum class BottleneckType {
        NONE,
        CPU_BOUND,
        GPU_BOUND,
        MEMORY_BOUND
    };

    struct AnalysisReport {
        BottleneckType bottleneck = BottleneckType::NONE;
        std::string primaryIssue;
        std::vector<std::string> suggestedOptimizations;
        bool canAutoApply = false;
    };

    // Engine Flags (Mocking the real engine systems for the analyzer to toggle)
    struct OptimizationFlags {
        bool enableGPUInstancing = false;
        bool enableNMMCulling = false;
        bool enableNFG = false;
        bool aggressiveMipmapping = false;
    };

    class NXCompare {
    public:
        // Analyze current telemetry data and return a diagnostic report
        AnalysisReport Analyze(const TelemetryData& currentData);

        // Automatically apply the suggested fixes from a report
        void ApplySuggestedOptimizations(const AnalysisReport& report, OptimizationFlags& engineFlags);

        // Convert bottleneck enum to string for UI
        static std::string BottleneckToString(BottleneckType type);
    };

}
