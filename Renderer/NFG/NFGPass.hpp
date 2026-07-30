#pragma once
#include <vector>
#include <memory>
#include "../../AI/Runtime/NAIRuntime.hpp"

namespace Cogent::Renderer::NFG {

    // Representation of a fully processed Frame ready for display
    struct FrameData {
        uint32_t frameId;
        bool isInterpolated;
        // In real vulkan, this would be VkImage or Texture handle
        void* textureHandle; 
    };

    class NFGPass {
    public:
        NFGPass();
        ~NFGPass();

        void Initialize();

        // Submits a natively rendered frame to the generator.
        // It returns a list of frames that should be presented to the screen.
        // Usually returns: [InterpolatedFrame(N-0.5), NativeFrame(N)]
        std::vector<FrameData> SubmitNativeFrame(uint32_t frameId, void* colorBuffer, void* motionVectors, void* depthBuffer);

    private:
        bool _initialized = false;
        FrameData _previousFrame;
        bool _hasPreviousFrame = false;
        
        // Caches the NAI Request to avoid allocations per frame
        std::shared_ptr<AI::Runtime::InferenceRequest> _frameGenRequest;
    };
}
