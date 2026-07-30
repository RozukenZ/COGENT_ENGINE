#include "FramePacer.hpp"
#include <thread>
#include <iostream>

namespace Cogent::Core::Diagnostics {

    void FramePacer::SetTargetFPS(double fps) {
        if (fps <= 0.0) {
            _targetFrameTimeMs = 0.0; // Uncapped
        } else {
            _targetFrameTimeMs = 1000.0 / fps;
        }
    }

    void FramePacer::BeginFrame() {
        _frameStartTime = std::chrono::high_resolution_clock::now();
    }

    void FramePacer::EndFrameAndPace() {
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = frameEndTime - _frameStartTime;

        if (_targetFrameTimeMs > 0.0) {
            double timeRemainingMs = _targetFrameTimeMs - elapsed.count();
            
            // If we are too fast, we need to pace (sleep)
            if (timeRemainingMs > 0.0) {
                // For maximum precision in our test, we use a 100% spin-lock. 
                // (In a real engine, we'd use timeBeginPeriod(1) and sleep_for the bulk).
                while (true) {
                    auto now = std::chrono::high_resolution_clock::now();
                    double currentElapsed = std::chrono::duration<double, std::milli>(now - _frameStartTime).count();
                    if (currentElapsed >= _targetFrameTimeMs) {
                        break;
                    }
                }
            }
        }

        auto postSleepTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> totalFrameTime = postSleepTime - _previousFrameEndTime;
        
        // If it's the very first frame, _previousFrameEndTime is uninitialized
        if (_previousFrameEndTime.time_since_epoch().count() == 0) {
            totalFrameTime = postSleepTime - _frameStartTime;
        }

        _lastFrameTimeMs = totalFrameTime.count();
        _previousFrameEndTime = postSleepTime;
    }
}
