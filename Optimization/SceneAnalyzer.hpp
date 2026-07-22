#pragma once
#include <cstdint>
#include <atomic>

namespace Cogent::Optimization {

    struct EngineStats {
        float frameTime = 0.0f; // ms
        uint32_t drawCalls = 0;
        uint32_t triangleCount = 0;
        uint32_t visibleObjects = 0;
        float gpuTime = 0.0f;   // ms
    };

    class SceneAnalyzer {
    public:
        static SceneAnalyzer& Get() {
            static SceneAnalyzer instance;
            return instance;
        }

        void resetFrame() {
            stats.drawCalls = 0;
            stats.triangleCount = 0;
            stats.visibleObjects = 0;
        }

        void registerDrawCall(uint32_t triCount) {
            stats.drawCalls++;
            stats.triangleCount += triCount;
        }

        void setFrameTime(float ms) {
            stats.frameTime = ms;
        }

        void setVisibleObjects(uint32_t count) {
            stats.visibleObjects = count;
        }

        const EngineStats& getStats() const { return stats; }

    private:
        EngineStats stats;
    };
}
