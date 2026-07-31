#include <iostream>
#include <iomanip>
#include "Core/Diagnostics/NXCompare.hpp"

using namespace Cogent::Core::Diagnostics;

void PrintReport(const AnalysisReport& report) {
    std::cout << "Detected Bottleneck : [" << NXCompare::BottleneckToString(report.bottleneck) << "]\n";
    std::cout << "Primary Issue       : " << report.primaryIssue << "\n";
    std::cout << "Suggested Fixes     :\n";
    for (const auto& fix : report.suggestedOptimizations) {
        std::cout << "  -> " << fix << "\n";
    }
    std::cout << "Auto-Apply Available: " << (report.canAutoApply ? "YES" : "NO") << "\n\n";
}

int main() {
    std::cout << "--- COGENT ENGINE NX COMPARE (ANALYZER) TEST ---\n\n";

    NXCompare analyzer;
    OptimizationFlags engineFlags; // Current state of the engine (all off initially)

    // ==========================================
    // CASE A: CPU BOUND (Too many draw calls)
    // ==========================================
    std::cout << ">> CASE A: Testing CPU Bound Scenario (Too many draw calls)\n";
    TelemetryData dataCaseA;
    dataCaseA.vramUsageBytes = 2ULL * 1024 * 1024 * 1024; // 2GB
    dataCaseA.cpuFrameTime = 22.0; // Slow CPU
    dataCaseA.gpuFrameTime = 10.0; // Fast GPU
    dataCaseA.totalFrameTime = 22.0;
    dataCaseA.drawCalls = 5400; // Too many draw calls

    AnalysisReport reportA = analyzer.Analyze(dataCaseA);
    PrintReport(reportA);

    if (reportA.bottleneck == BottleneckType::CPU_BOUND && reportA.canAutoApply) {
        std::cout << "[PASS] Correctly diagnosed CPU Bound due to draw calls.\n";
        
        // User clicks "Apply All"
        std::cout << "[ACTION] Applying optimizations...\n";
        analyzer.ApplySuggestedOptimizations(reportA, engineFlags);
        
        if (engineFlags.enableGPUInstancing && engineFlags.enableNMMCulling) {
            std::cout << "[PASS] Auto-apply successfully toggled GPU Instancing and NMM Culling!\n";
        } else {
            std::cerr << "[FAIL] Flags were not correctly toggled.\n";
            return 1;
        }
    } else {
        std::cerr << "[FAIL] Failed to diagnose Case A.\n";
        return 1;
    }

    std::cout << "------------------------------------------\n";

    // ==========================================
    // CASE B: MEMORY BOUND (VRAM Overflow)
    // ==========================================
    std::cout << ">> CASE B: Testing Memory Bound Scenario (VRAM Overflow)\n";
    TelemetryData dataCaseB;
    dataCaseB.vramUsageBytes = 7ULL * 1024 * 1024 * 1024; // 7GB VRAM! (Threshold is 6GB)
    dataCaseB.cpuFrameTime = 16.0; 
    dataCaseB.gpuFrameTime = 16.0; 
    dataCaseB.totalFrameTime = 16.0;
    
    AnalysisReport reportB = analyzer.Analyze(dataCaseB);
    PrintReport(reportB);

    if (reportB.bottleneck == BottleneckType::MEMORY_BOUND) {
        std::cout << "[PASS] Correctly diagnosed Memory Bound.\n";
        analyzer.ApplySuggestedOptimizations(reportB, engineFlags);
        if (engineFlags.aggressiveMipmapping) {
            std::cout << "[PASS] Auto-apply successfully forced Aggressive Mipmapping!\n";
        } else {
            std::cerr << "[FAIL] Memory optimizations failed to apply.\n";
            return 1;
        }
    } else {
        std::cerr << "[FAIL] Failed to diagnose Case B.\n";
        return 1;
    }

    std::cout << "\n[FINAL PASS] NX Compare successfully diagnosed and resolved all scenarios!\n";

    return 0;
}
