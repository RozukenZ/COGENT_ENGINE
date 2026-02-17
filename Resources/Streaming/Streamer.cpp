#include "Streamer.hpp"
#include "../../Core/Threading/JobSystem.hpp"
#include <algorithm>
#include <iostream>

namespace Cogent::Resources {

    Streamer::Streamer(GraphicsDevice& device) : device(device) {
    }

    Streamer::~Streamer() {
        // Ensure all resources are unloaded or handled
        for (auto& res : resources) {
            if (res->state == StreamingState::RESIDENT) {
                res->unload();
            }
        }
    }

    void Streamer::registerResource(std::shared_ptr<StreamableResource> resource) {
        std::lock_guard<std::mutex> lock(queueMutex);
        resources.push_back(resource);
    }

    void Streamer::requestLoad(std::shared_ptr<StreamableResource> resource) {
        if (resource->state == StreamingState::UNLOADED) {
            std::lock_guard<std::mutex> lock(queueMutex);
            resource->state = StreamingState::PENDING_LOAD;
            loadQueue.push_back(resource);
        }
    }

    void Streamer::update(const glm::vec3& cameraPos, float deltaTime) {
        updatePriorities(cameraPos);
        processQueues();
        // unloadUnused(); // Optional: Implement unload logic based on timer
    }

    void Streamer::updatePriorities(const glm::vec3& cameraPos) {
        // Simple priority update based on last used time (LRU) or specific game logic
        // For now, sorting queue by priority is good enough.
        // We could verify distance here if resources had world positions attached.
        
        std::lock_guard<std::mutex> lock(queueMutex);
        std::sort(loadQueue.begin(), loadQueue.end(), [](const auto& a, const auto& b) {
            return a->priority > b->priority;
        });
    }

    void Streamer::processQueues() {
        // 1. Check for Pending Loads -> Dispatch to JobSystem
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!loadQueue.empty()) {
                auto res = loadQueue.front();
                
                // Only dispatch if not already processed
                if (res->state == StreamingState::PENDING_LOAD) {
                    res->state = StreamingState::LOADING; 
                    
                    Threading::JobSystem::Get().Execute([this, res]() {
                        res->loadCPU();
                        
                        std::lock_guard<std::mutex> lock(queueMutex);
                        if (res->state == StreamingState::LOADING) { // Check if cancelled
                            res->state = StreamingState::LOADED_CPU;
                            uploadQueue.push_back(res);
                        }
                    });
                    
                    loadQueue.pop_front();
                }
            }
        }

        // 2. Check for Uploads -> Execute on Main Thread
        // (Limit based on time or bytes - naive implementation)
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!uploadQueue.empty()) {
                auto res = uploadQueue.front();
                if (res->state == StreamingState::LOADED_CPU) {
                     res->uploadGPU(device); 
                     // State is updated to RESIDENT inside uploadGPU usually, or we force it here
                     if (res->state != StreamingState::RESIDENT) res->state = StreamingState::RESIDENT;
                     
                     // LOG_INFO("Streamed In Resource: " + res->path);
                     
                     uploadQueue.pop_front();
                }
            }
        }
    }
    
    void Streamer::unloadUnused() {
        // Iterate through resources, check lastUsedTime.
        // For now, since we don't track usage frame-by-frame on resources yet (need reference counting or touch()), 
        // we will leave this as a placeholder or implement simple time-based if 'lastUsedTime' is updated.
        // Assuming 'lastUsedTime' is updated by Renderer when using resource.
        
        // Example logic:
        // float currentTime = (float)glfwGetTime(); // Need access to time
        // for (auto& res : resources) {
        //    if (res->state == StreamingState::RESIDENT && (currentTime - res->lastUsedTime > unloadTimeout)) {
        //        res->unload();
        //    }
        // }
    }
}
