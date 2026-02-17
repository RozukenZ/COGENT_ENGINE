#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include "../../Core/Types.hpp"
#include "../../Core/Graphics/GraphicsDevice.hpp"

namespace Cogent::Resources {

    enum class StreamingState {
        UNLOADED,
        PENDING_LOAD,
        LOADING,
        LOADED_CPU,
        UPLOADING,
        RESIDENT // GPU Resident
    };

    class StreamableResource {
    public:
        std::string path;
        StreamingState state = StreamingState::UNLOADED;
        float priority = 0.0f;
        float lastUsedTime = 0.0f;
        
        virtual void loadCPU() = 0;
        virtual void uploadGPU(GraphicsDevice& device) = 0;
        virtual void unload() = 0;
        virtual ~StreamableResource() = default;
    };

    class Streamer {
    public:
        Streamer(GraphicsDevice& device);
        ~Streamer();

        void update(const glm::vec3& cameraPos, float deltaTime);
        
        void registerResource(std::shared_ptr<StreamableResource> resource);
        void requestLoad(std::shared_ptr<StreamableResource> resource);

    private:
        void updatePriorities(const glm::vec3& cameraPos);
        void processQueues();
        void unloadUnused();

        GraphicsDevice& device;
        std::vector<std::shared_ptr<StreamableResource>> resources;
        
        std::deque<std::shared_ptr<StreamableResource>> loadQueue;
        std::deque<std::shared_ptr<StreamableResource>> uploadQueue;
        std::mutex queueMutex;

        // Settings
        float maxUploadBudgetPerFrame = 5.0f * 1024.0f * 1024.0f; // 5MB per frame
        float unloadTimeout = 30.0f; // Unload if not seen for 30s
    };
}
