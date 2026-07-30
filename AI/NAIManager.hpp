#pragma once
#include <string>
#include <memory>
#include <atomic>
#include "../Core/Threading/JobSystem.hpp"
#include "BehaviorTree.hpp"

namespace Cogent::AI {

    class NAIManager {
    public:
        static NAIManager& Get() {
            static NAIManager instance;
            return instance;
        }

        // Submits a heavy ML task to the JobSystem asynchronously.
        // It updates the Blackboard when done.
        void SubmitInferenceTask(Blackboard& blackboard, const std::string& bbKey) {
            
            // Mark task as RUNNING in blackboard so BT knows it's being processed
            blackboard.Set(bbKey + "_Status", std::string("RUNNING"));
            
            auto& jobSystem = Threading::JobSystem::Get();
            jobSystem.Execute([&blackboard, bbKey]() {
                // MOCK ML INFERENCE (e.g. Vision Model or LLM Prompt)
                // In production, this would call ONNX Runtime or TensorRT
                
                // Simulate heavy work
                volatile double dummy = 0.0;
                for(int i=0; i<10000; i++) dummy += 1.0; 
                
                // Update Blackboard thread-safely
                blackboard.Set(bbKey + "_Status", std::string("SUCCESS"));
                blackboard.Set(bbKey + "_Result", std::string("TargetSpotted"));
            });
        }

    private:
        NAIManager() = default;
    };

}
