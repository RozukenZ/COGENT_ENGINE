#pragma once
#include <chrono>

namespace Cogent::Core::Diagnostics {

    class FramePacer {
    public:
        // Set the target FPS (e.g. 60.0f)
        void SetTargetFPS(double fps);

        // Call this at the start of the frame
        void BeginFrame();

        // Call this at the very end of the frame, right before present
        // It will sleep the thread just enough to hit the target frame time
        void EndFrameAndPace();

        // Returns the actual time the last frame took (for metrics)
        double GetLastFrameTimeMs() const { return _lastFrameTimeMs; }

    private:
        double _targetFrameTimeMs = 16.666667; // Default 60 FPS
        double _lastFrameTimeMs = 0.0;
        
        std::chrono::high_resolution_clock::time_point _frameStartTime;
        std::chrono::high_resolution_clock::time_point _previousFrameEndTime;
    };
}
