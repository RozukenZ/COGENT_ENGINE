#include "NXCompare.hpp"

namespace Cogent::Core::Diagnostics {

    std::string NXCompare::BottleneckToString(BottleneckType type) {
        switch (type) {
            case BottleneckType::CPU_BOUND: return "CPU BOUND";
            case BottleneckType::GPU_BOUND: return "GPU BOUND";
            case BottleneckType::MEMORY_BOUND: return "MEMORY BOUND";
            default: return "NONE / OPTIMAL";
        }
    }

    AnalysisReport NXCompare::Analyze(const TelemetryData& currentData) {
        AnalysisReport report;
        
        // Target: 60 FPS (16.66ms per frame) for basic heuristics
        const double TARGET_FRAME_TIME = 16.66;
        
        // 1. Check Memory Bound
        // Threshold: VRAM > 6.0GB (6 * 1024^3 bytes)
        const uint64_t VRAM_WARNING_THRESHOLD = 6ULL * 1024 * 1024 * 1024; 
        if (currentData.vramUsageBytes > VRAM_WARNING_THRESHOLD) {
            report.bottleneck = BottleneckType::MEMORY_BOUND;
            report.primaryIssue = "Critical VRAM allocation detected. Texture pools are overflowing.";
            report.suggestedOptimizations.push_back("Enable Aggressive Mipmapping");
            report.canAutoApply = true;
            return report; // Return early, memory is most critical
        }

        // 2. Check GPU Bound vs CPU Bound
        if (currentData.totalFrameTime > TARGET_FRAME_TIME) {
            // Is CPU slower than GPU?
            if (currentData.cpuFrameTime > currentData.gpuFrameTime) {
                report.bottleneck = BottleneckType::CPU_BOUND;
                
                if (currentData.drawCalls > 2000) {
                    report.primaryIssue = "Draw call overhead is severely blocking the Render Thread.";
                    report.suggestedOptimizations.push_back("Enable GPU Auto-Instancing");
                    report.suggestedOptimizations.push_back("Enable NMM Culling");
                    report.canAutoApply = true;
                } else {
                    report.primaryIssue = "Heavy gameplay logic or physics overhead.";
                }
            } else {
                // GPU is slower
                report.bottleneck = BottleneckType::GPU_BOUND;
                report.primaryIssue = "GPU is struggling to rasterize or trace the scene.";
                report.suggestedOptimizations.push_back("Enable Neural Frame Generation (NFG)");
                report.canAutoApply = true;
            }
        } else {
            report.bottleneck = BottleneckType::NONE;
            report.primaryIssue = "Engine is running smoothly.";
        }

        return report;
    }

    void NXCompare::ApplySuggestedOptimizations(const AnalysisReport& report, OptimizationFlags& engineFlags) {
        if (!report.canAutoApply) return;

        for (const auto& suggestion : report.suggestedOptimizations) {
            if (suggestion == "Enable GPU Auto-Instancing") engineFlags.enableGPUInstancing = true;
            if (suggestion == "Enable NMM Culling") engineFlags.enableNMMCulling = true;
            if (suggestion == "Enable Neural Frame Generation (NFG)") engineFlags.enableNFG = true;
            if (suggestion == "Enable Aggressive Mipmapping") engineFlags.aggressiveMipmapping = true;
        }
    }
}
