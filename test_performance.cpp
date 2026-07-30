#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <numeric>
#include <cmath>
#include "Core/Diagnostics/FramePacer.hpp"

using namespace Cogent::Core::Diagnostics;

void SimulateWorkload(int milliseconds) {
    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= milliseconds) {
            break;
        }
    }
}

int main() {
    std::cout << "--- COGENT ENGINE NRL (FRAME PACING) STRESS TEST ---\n";
    
    FramePacer pacer;
    
    // We target 60 FPS (16.66 ms per frame)
    double targetFPS = 60.0;
    pacer.SetTargetFPS(targetFPS);
    std::cout << "[INFO] Target FPS set to: " << targetFPS << " (16.666 ms)\n";

    const int NUM_FRAMES = 100;
    std::vector<double> frameTimes;
    frameTimes.reserve(NUM_FRAMES);

    std::cout << "[TEST] Simulating " << NUM_FRAMES << " frames with varying workload (3ms to 12ms)...\n";
    
    // Dummy variables for variance
    for (int i = 0; i < NUM_FRAMES; ++i) {
        pacer.BeginFrame();
        
        // Simulate a game engine workload that fluctuates. 
        // e.g. One frame takes 3ms, another takes 12ms.
        int simulatedWorkMs = 3 + (i % 10); 
        SimulateWorkload(simulatedWorkMs);
        
        pacer.EndFrameAndPace();
        
        // Skip the very first frame time as it's an initialization frame
        if (i > 0) {
            frameTimes.push_back(pacer.GetLastFrameTimeMs());
        }
    }

    // Analyze results
    double sum = 0.0;
    double maxTime = 0.0;
    double minTime = 999.0;
    
    for (double time : frameTimes) {
        sum += time;
        if (time > maxTime) maxTime = time;
        if (time < minTime) minTime = time;
    }
    
    double avgTime = sum / frameTimes.size();
    
    // Calculate Variance & Jitter
    double varianceSum = 0.0;
    for (double time : frameTimes) {
        varianceSum += (time - avgTime) * (time - avgTime);
    }
    double jitter = std::sqrt(varianceSum / frameTimes.size());
    
    std::cout << "\n--- PACING RESULTS ---\n";
    std::cout << "Average Frame Time : " << avgTime << " ms\n";
    std::cout << "Min Frame Time     : " << minTime << " ms\n";
    std::cout << "Max Frame Time     : " << maxTime << " ms\n";
    std::cout << "Jitter (Variance)  : " << jitter << " ms\n";

    // If jitter is extremely low (e.g. < 2.0 ms), pacing is solid!
    if (jitter < 2.0) {
        std::cout << "[PASS] Frame Pacing is extremely stable (AOS successful)!\n";
    } else {
        std::cerr << "[FAIL] Micro-stuttering detected. Jitter is too high!\n";
        return 1;
    }

    return 0;
}
