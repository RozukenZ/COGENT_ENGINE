#pragma once
#include <chrono>
#include <thread>

namespace Cogent::Core {

    class FramePacer {
    public:
        FramePacer(int targetFPS = 60) {
            setTargetFPS(targetFPS);
        }

        void setTargetFPS(int fps) {
            targetDuration = std::chrono::duration<double, std::milli>(1000.0 / fps);
        }

        void beginFrame() {
            startTime = std::chrono::high_resolution_clock::now();
        }

        void endFrame() {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = endTime - startTime;

            if (duration < targetDuration) {
                // Sleep specifically for the remainder
                // Note: std::this_thread::sleep_for can be imprecise on Windows
                // A better approach involves spin-waiting the last millisecond
                std::this_thread::sleep_for(targetDuration - duration);
            }
        }

    private:
        std::chrono::duration<double, std::milli> targetDuration;
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    };
}
