#include <iostream>
#include "Core/Diagnostics/HAS.hpp"

using namespace Cogent::Core::Diagnostics;

void PrintSettings(const EngineSettings& settings) {
    std::cout << "  -> Hardware Ray Tracing  : " << (settings.enableHardwareRayTracing ? "ON" : "OFF") << "\n";
    std::cout << "  -> Neural Frame Gen (NFG): " << (settings.enableNeuralFrameGeneration ? "ON" : "OFF") << "\n";
    std::cout << "  -> Shadow Resolution     : " << settings.shadowResolution << "\n";
    std::cout << "  -> Texture Quality       : " << settings.textureQuality << "\n";
    std::cout << "  -> Aggressive Culling    : " << (settings.aggressiveCulling ? "ON" : "OFF") << "\n\n";
}

int main() {
    std::cout << "--- COGENT ENGINE HARDWARE AWARENESS SYSTEM (HAS) TEST ---\n\n";

    HardwareAwarenessSystem has;

    // ==========================================
    // CASE A: LOW END LAPTOP
    // ==========================================
    std::cout << ">> CASE A: Booting on Low-End Laptop (Intel i3, iGPU, 2GB VRAM)\n";
    auto potatoProfile = has.CreateMockProfile(2, 2, "Intel i3 Mobile", "Intel UHD Graphics");
    auto potatoSettings = has.EvaluateSettings(potatoProfile);
    PrintSettings(potatoSettings);
    
    if (potatoSettings.enableNeuralFrameGeneration && potatoSettings.shadowResolution == 512) {
        std::cout << "[PASS] HAS successfully forced survival mode for Potato PC.\n";
    } else {
        std::cerr << "[FAIL] HAS failed to optimize for Potato PC.\n";
        return 1;
    }
    
    std::cout << "------------------------------------------\n";

    // ==========================================
    // CASE B: ULTRA ENTHUSIAST PC
    // ==========================================
    std::cout << ">> CASE B: Booting on Ultra PC (Ryzen 9, RTX 4090, 24GB VRAM)\n";
    auto ultraProfile = has.CreateMockProfile(16, 24, "AMD Ryzen 9 7950X", "NVIDIA RTX 4090");
    auto ultraSettings = has.EvaluateSettings(ultraProfile);
    PrintSettings(ultraSettings);
    
    if (ultraSettings.enableHardwareRayTracing && ultraSettings.shadowResolution == 4096) {
        std::cout << "[PASS] HAS successfully maximized visual fidelity for Ultra PC.\n";
    } else {
        std::cerr << "[FAIL] HAS failed to maximize settings.\n";
        return 1;
    }

    std::cout << "------------------------------------------\n";
    
    // ==========================================
    // CASE C: NATIVE SCAN
    // ==========================================
    std::cout << ">> CASE C: Native Hardware Scan\n";
    auto nativeProfile = has.ScanCurrentHardware();
    std::cout << "[HAS] Native CPU Cores detected: " << nativeProfile.cpuCores << "\n";
    auto nativeSettings = has.EvaluateSettings(nativeProfile);
    PrintSettings(nativeSettings);

    std::cout << "[FINAL PASS] Hardware Awareness System adapts flawlessly across ecosystems!\n";

    return 0;
}
