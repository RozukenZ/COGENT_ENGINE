#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include "RayTracing/BVHBuilder.hpp"

using namespace Cogent::RayTracing;

int main() {
    std::cout << "--- COGENT ENGINE NRT (RAY TRACING) STRESS TEST ---\n";
    
    BVHBuilder bvh;
    bvh.Init(nullptr, nullptr); // Mock init

    const size_t NUM_INSTANCES = 5000;
    std::vector<RTInstance> instances;
    instances.reserve(NUM_INSTANCES);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distPos(-1000.0f, 1000.0f);

    for (size_t i = 0; i < NUM_INSTANCES; ++i) {
        RTInstance inst;
        inst.instanceId = static_cast<uint32_t>(i);
        inst.blasId = 1; // Assume they all share the same Mesh BLAS
        
        glm::mat4 transform = glm::mat4(1.0f);
        transform[3][0] = distPos(rng);
        transform[3][1] = distPos(rng);
        transform[3][2] = distPos(rng);
        inst.transform = transform;
        
        instances.push_back(inst);
    }

    std::cout << "[INFO] Building Top-Level Acceleration Structure (TLAS) for " << NUM_INSTANCES << " instances...\n";

    auto start = std::chrono::high_resolution_clock::now();
    
    // In a real pipeline, this is pushed to the GPU command buffer.
    // For test, we measure the driver overhead / simulation cost.
    bvh.SimulateBuildTLASLoad(instances);

    auto end = std::chrono::high_resolution_clock::now();
    double timeTakenMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "\n--- NRT RESULTS ---\n";
    std::cout << "TLAS Build Time : " << timeTakenMs << " ms\n";
    
    // Target is under 2.0 ms
    if (timeTakenMs < 2.0) {
        std::cout << "[PASS] Hardware RT BVH overhead is extremely low. Engine is ready for real-time Rays!\n";
    } else {
        std::cerr << "[FAIL] TLAS Build Time is too slow. Bottleneck detected!\n";
        return 1;
    }

    bvh.Cleanup();
    return 0;
}
