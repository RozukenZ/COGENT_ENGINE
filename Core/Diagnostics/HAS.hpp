#pragma once
#include <string>

namespace Cogent::Core::Diagnostics {

    enum class HardwareTier {
        LOW_END,     // e.g. iGPU, 2 Cores, 2GB VRAM
        MID_RANGE,   // e.g. GTX 1060, 4 Cores, 6GB VRAM
        HIGH_END,    // e.g. RTX 3060, 6 Cores, 8GB VRAM
        ULTRA        // e.g. RTX 4090, 16 Cores, 24GB VRAM
    };

    struct HardwareProfile {
        std::string cpuModel;
        std::string gpuModel;
        uint32_t cpuCores = 0;
        uint64_t vramCapacityBytes = 0;
        uint64_t systemRamBytes = 0;
        
        HardwareTier classification = HardwareTier::MID_RANGE;
    };

    struct EngineSettings {
        bool enableHardwareRayTracing = false;
        bool enableNeuralFrameGeneration = false;
        int shadowResolution = 1024;
        bool aggressiveCulling = false;
        std::string textureQuality = "Medium";
    };

    class HardwareAwarenessSystem {
    public:
        // Scans the current machine to build a profile (Mock implementation for now)
        HardwareProfile ScanCurrentHardware();

        // Overrides the scan (useful for testing different PC specs)
        HardwareProfile CreateMockProfile(uint32_t cores, uint64_t vramGB, const std::string& cpu, const std::string& gpu);

        // Uses the profile to automatically define engine settings
        EngineSettings EvaluateSettings(const HardwareProfile& profile);

    private:
        HardwareTier ClassifyProfile(const HardwareProfile& profile);
    };

}
