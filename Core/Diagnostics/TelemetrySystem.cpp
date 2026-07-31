#include "TelemetrySystem.hpp"
#include <iostream>

namespace Cogent::Core::Diagnostics {

    void TelemetrySystem::Initialize() {
        ResetPerFrameMetrics();
        std::cout << "[TELEMETRY] NX Insight System Initialized.\n";
    }

    void TelemetrySystem::Shutdown() {
        std::cout << "[TELEMETRY] NX Insight System Shutdown.\n";
    }

    void TelemetrySystem::ResetPerFrameMetrics() {
        _drawCalls.store(0, std::memory_order_relaxed);
        _triangleCount.store(0, std::memory_order_relaxed);
        _vertexCount.store(0, std::memory_order_relaxed);
    }

    void TelemetrySystem::SetMode(TelemetryMode mode) {
        _currentMode = mode;
    }

    void TelemetrySystem::AddDrawCall(uint32_t triangles, uint32_t vertices) {
        // Atomic fetch_add is thread-safe and lock-free on most architectures
        _drawCalls.fetch_add(1, std::memory_order_relaxed);
        _triangleCount.fetch_add(triangles, std::memory_order_relaxed);
        _vertexCount.fetch_add(vertices, std::memory_order_relaxed);
    }

    void TelemetrySystem::UpdateMemory(uint64_t allocatedRam, uint64_t allocatedVram) {
        _ramUsageBytes.store(allocatedRam, std::memory_order_relaxed);
        _vramUsageBytes.store(allocatedVram, std::memory_order_relaxed);
    }

    void TelemetrySystem::UpdateTimings(double gpuTime, double cpuTime, double totalTime, double fps) {
        // We assume this is called once per frame at the end by the Main Thread, so no locks needed.
        _gpuFrameTime = gpuTime;
        _cpuFrameTime = cpuTime;
        _totalFrameTime = totalTime;
        _effectiveFps = fps;
    }

    TelemetryData TelemetrySystem::GetCurrentData() const {
        TelemetryData data;
        data.ramUsageBytes = _ramUsageBytes.load(std::memory_order_relaxed);
        data.vramUsageBytes = _vramUsageBytes.load(std::memory_order_relaxed);
        data.drawCalls = _drawCalls.load(std::memory_order_relaxed);
        data.triangleCount = _triangleCount.load(std::memory_order_relaxed);
        data.vertexCount = _vertexCount.load(std::memory_order_relaxed);
        data.gpuFrameTime = _gpuFrameTime;
        data.cpuFrameTime = _cpuFrameTime;
        data.totalFrameTime = _totalFrameTime;
        data.effectiveFps = _effectiveFps;
        return data;
    }

    double TelemetrySystem::CalculateGainPercentage(double rawFps, double optimizedFps) const {
        if (rawFps <= 0.0) return 0.0;
        return ((optimizedFps - rawFps) / rawFps) * 100.0;
    }
}
