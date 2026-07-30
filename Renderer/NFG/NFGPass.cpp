#include "NFGPass.hpp"
#include <iostream>

namespace Cogent::Renderer::NFG {

    NFGPass::NFGPass() {
    }

    NFGPass::~NFGPass() {
    }

    void NFGPass::Initialize() {
        // Prepare inference request for NAIRuntime
        _frameGenRequest = std::make_shared<AI::Runtime::InferenceRequest>();
        _frameGenRequest->modelName = "FrameGen_NFG.trt";
        // Inputs would normally be Color, Depth, MV
        
        _initialized = true;
        std::cout << "[NFG] Neural Frame Generation Engine Initialized.\n";
    }

    std::vector<FrameData> NFGPass::SubmitNativeFrame(uint32_t frameId, void* colorBuffer, void* motionVectors, void* depthBuffer) {
        std::vector<FrameData> framesToPresent;

        FrameData currentNative;
        currentNative.frameId = frameId;
        currentNative.isInterpolated = false;
        currentNative.textureHandle = colorBuffer;

        if (!_hasPreviousFrame) {
            // Can't interpolate without a previous frame
            framesToPresent.push_back(currentNative);
            _previousFrame = currentNative;
            _hasPreviousFrame = true;
            return framesToPresent;
        }

        // 1. Submit Generation Task to NAI Runtime
        _frameGenRequest->isCompleted = false;
        AI::Runtime::NAIRuntime::Get().SubmitInference(_frameGenRequest);

        // 2. Force Flush for immediate generation (In a real engine, we pipeline this over frames, but here we wait to simulate cost)
        AI::Runtime::NAIRuntime::Get().FlushQueue();
        
        // Busy wait for completion (simulate GPU Sync / Fence wait)
        while (!_frameGenRequest->isCompleted) {
            // In a real engine, we'd do other work or wait on a Vulkan Fence here
        }

        // 3. Create the Interpolated Frame (N-0.5)
        FrameData interpolatedFrame;
        interpolatedFrame.frameId = frameId; // Technically belongs to this submission cycle
        interpolatedFrame.isInterpolated = true;
        interpolatedFrame.textureHandle = nullptr; // Mock output

        // 4. Queue presentation order: Interpolated FIRST, then Native SECOND
        framesToPresent.push_back(interpolatedFrame);
        framesToPresent.push_back(currentNative);

        _previousFrame = currentNative;
        return framesToPresent;
    }
}
