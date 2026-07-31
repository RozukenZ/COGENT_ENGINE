#include "HAS.hpp"
#include <thread>
#include <iostream>

namespace Cogent::Core::Diagnostics {

    HardwareTier HardwareAwarenessSystem::ClassifyProfile(const HardwareProfile& profile) {
        // Simple logic for classification
        const uint64_t GB = 1024ULL * 1024 * 1024;
        
        if (profile.vramCapacityBytes <= 4 * GB || profile.cpuCores <= 4) {
            return HardwareTier::LOW_END;
        } 
        else if (profile.vramCapacityBytes >= 16 * GB && profile.cpuCores >= 12) {
            return HardwareTier::ULTRA;
        }
        else if (profile.vramCapacityBytes >= 8 * GB && profile.cpuCores >= 6) {
            return HardwareTier::HIGH_END;
        }
        
        return HardwareTier::MID_RANGE;
    }

    HardwareProfile HardwareAwarenessSystem::CreateMockProfile(uint32_t cores, uint64_t vramGB, const std::string& cpu, const std::string& gpu) {
        HardwareProfile profile;
        profile.cpuCores = cores;
        profile.vramCapacityBytes = vramGB * 1024 * 1024 * 1024;
        profile.cpuModel = cpu;
        profile.gpuModel = gpu;
        profile.classification = ClassifyProfile(profile);
        return profile;
    }

    HardwareProfile HardwareAwarenessSystem::ScanCurrentHardware() {
        // In a real implementation, we'd query DXGI/Vulkan and OS APIs
        // Here we mock using std::thread
        HardwareProfile profile;
        profile.cpuCores = std::thread::hardware_concurrency();
        if (profile.cpuCores == 0) profile.cpuCores = 4; // Fallback
        
        // Mock VRAM (Let's assume a Mid-Range machine for the real scan)
        profile.vramCapacityBytes = 6ULL * 1024 * 1024 * 1024; 
        
        profile.cpuModel = "Native CPU Scanner";
        profile.gpuModel = "Native GPU Scanner";
        profile.classification = ClassifyProfile(profile);
        
        return profile;
    }

    EngineSettings HardwareAwarenessSystem::EvaluateSettings(const HardwareProfile& profile) {
        EngineSettings settings;
        
        std::cout << "[HAS] Hardware Classified as: ";
        switch (profile.classification) {
            case HardwareTier::LOW_END:
                std::cout << "LOW END (Potato / iGPU)\n";
                // Potato PC Survival Mode
                settings.enableHardwareRayTracing = false;
                settings.enableNeuralFrameGeneration = true; // Use AI to fake frames
                settings.shadowResolution = 512;
                settings.aggressiveCulling = true;
                settings.textureQuality = "Low";
                break;
                
            case HardwareTier::MID_RANGE:
                std::cout << "MID RANGE\n";
                settings.enableHardwareRayTracing = false;
                settings.enableNeuralFrameGeneration = false;
                settings.shadowResolution = 1024;
                settings.aggressiveCulling = false;
                settings.textureQuality = "Medium";
                break;
                
            case HardwareTier::HIGH_END:
                std::cout << "HIGH END\n";
                settings.enableHardwareRayTracing = true; // Enough VRAM for BVH
                settings.enableNeuralFrameGeneration = false;
                settings.shadowResolution = 2048;
                settings.aggressiveCulling = false;
                settings.textureQuality = "High";
                break;
                
            case HardwareTier::ULTRA:
                std::cout << "ULTRA (Enthusiast)\n";
                settings.enableHardwareRayTracing = true;
                settings.enableNeuralFrameGeneration = false; // Pure raster + RT
                settings.shadowResolution = 4096; // 4K shadows
                settings.aggressiveCulling = false;
                settings.textureQuality = "Ultra";
                break;
        }
        
        return settings;
    }

}
