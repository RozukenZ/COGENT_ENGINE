#include "BVHBuilder.hpp"
#include <iostream>
#include <chrono>
#include <thread>

namespace Cogent::RayTracing {

    bool BVHBuilder::CheckHardwareRTSupport(VkPhysicalDevice physicalDevice) {
        // Query Vulkan Device Extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

        bool hasAccelStruct = false;
        bool hasRTPipeline = false;

        for (const auto& extension : availableExtensions) {
            if (strcmp(extension.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
                hasAccelStruct = true;
            }
            if (strcmp(extension.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
                hasRTPipeline = true;
            }
        }

        return hasAccelStruct && hasRTPipeline;
    }

    void BVHBuilder::Init(VkDevice device, VkPhysicalDevice physicalDevice) {
        _device = device;
        _physicalDevice = physicalDevice;
        std::cout << "[NRT] Initialized BVH Builder.\n";
    }

    void BVHBuilder::Cleanup() {
        // Cleanup Vulkan Handles here
    }

    uint32_t BVHBuilder::BuildBLAS(VkCommandBuffer cmd, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
        // In a real implementation: vkCmdBuildAccelerationStructuresKHR(...)
        return _nextBlasId++;
    }

    void BVHBuilder::BuildTLAS(VkCommandBuffer cmd, const std::vector<RTInstance>& instances) {
        // In a real implementation: vkCmdBuildAccelerationStructuresKHR(...) for TLAS
    }

    void BVHBuilder::SimulateBuildTLASLoad(const std::vector<RTInstance>& instances) {
        // Karena Vulkan KHR Setup butuh 1000+ baris boiler plate dan Vulkan SDK terbaru (1.3+),
        // Kita mensimulasikan computational cost dari membangun TLAS di GPU (dikalkulasi via CPU untuk metrics)
        // Di mana cost linear terhadap jumlah instance.
        
        volatile double dummy = 0.0;
        for (const auto& inst : instances) {
            // Simulasi transformasi matrix dan traversal cost
            for(int i = 0; i < 50; i++) {
                dummy += inst.transform[0][0] * 1.01;
            }
        }
    }
}
