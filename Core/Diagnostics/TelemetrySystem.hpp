#pragma once
#include <cstdint>
#include <atomic>

namespace Cogent::Core::Diagnostics {

    enum class TelemetryMode {
        RAW,
        OPTIMIZED
    };

    struct TelemetryData {
        // Memory
        uint64_t ramUsageBytes = 0;
        uint64_t vramUsageBytes = 0;

        // Rendering Stats
        uint32_t drawCalls = 0;
        uint32_t triangleCount = 0;
        uint32_t vertexCount = 0;

        // Timing (ms)
        double gpuFrameTime = 0.0;
        double cpuFrameTime = 0.0;
        double totalFrameTime = 0.0;
        double effectiveFps = 0.0;
    };

    class TelemetrySystem {
    public:
        static TelemetrySystem& Get() {
            static TelemetrySystem instance;
            return instance;
        }

        void Initialize();
        void Shutdown();
        void ResetPerFrameMetrics();

        // Mode Switching
        void SetMode(TelemetryMode mode);
        TelemetryMode GetMode() const { return _currentMode; }

        // Thread-safe metric updates
        void AddDrawCall(uint32_t triangles, uint32_t vertices);
        void UpdateMemory(uint64_t allocatedRam, uint64_t allocatedVram);
        void UpdateTimings(double gpuTime, double cpuTime, double totalTime, double fps);

        // Fetch Data
        TelemetryData GetCurrentData() const;
        
        // Return percentage gain (e.g., +150%) comparing Raw vs Optimized fps
        double CalculateGainPercentage(double rawFps, double optimizedFps) const;

    private:
        TelemetrySystem() = default;

        TelemetryMode _currentMode = TelemetryMode::OPTIMIZED;

        // Atomic counters for thread safety during multithreaded render passes (e.g. Phase 2)
        std::atomic<uint32_t> _drawCalls{0};
        std::atomic<uint32_t> _triangleCount{0};
        std::atomic<uint32_t> _vertexCount{0};

        std::atomic<uint64_t> _ramUsageBytes{0};
        std::atomic<uint64_t> _vramUsageBytes{0};

        // Standard locks for double precision variables (or we could use std::atomic<double> in C++20)
        double _gpuFrameTime = 0.0;
        double _cpuFrameTime = 0.0;
        double _totalFrameTime = 0.0;
        double _effectiveFps = 0.0;
    };
}
