#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include "../Core/Types.hpp"

namespace Cogent::RayTracing {

    // Representasi dari Instance di dalam Top-Level Acceleration Structure
    struct RTInstance {
        uint32_t instanceId;
        uint32_t blasId;
        glm::mat4 transform;
    };

    class BVHBuilder {
    public:
        // Cek apakah ekstensi VK_KHR_ray_tracing_pipeline dan VK_KHR_acceleration_structure tersedia
        static bool CheckHardwareRTSupport(VkPhysicalDevice physicalDevice);

        void Init(VkDevice device, VkPhysicalDevice physicalDevice);
        void Cleanup();

        // Build Bottom-Level Acceleration Structure (Statik, hanya dipanggil sekali per Mesh)
        uint32_t BuildBLAS(VkCommandBuffer cmd, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        
        // Build Top-Level Acceleration Structure (Dinamis, dipanggil setiap frame untuk objek bergerak)
        void BuildTLAS(VkCommandBuffer cmd, const std::vector<RTInstance>& instances);

        // Simulasi build (karena setup Vulkan KHR extensions sangat panjang untuk MVP)
        // Fungsi ini hanya mensimulasikan beban kerja untuk test unit
        void SimulateBuildTLASLoad(const std::vector<RTInstance>& instances);

    private:
        VkDevice _device = VK_NULL_HANDLE;
        VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;

        // Dummy handles untuk abstraksi
        VkBuffer _tlasBuffer = VK_NULL_HANDLE;
        VkDeviceMemory _tlasMemory = VK_NULL_HANDLE;
        
        uint32_t _nextBlasId = 1;
    };

}
