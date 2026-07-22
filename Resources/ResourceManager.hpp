#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <mutex>
#include "../Threading/JobSystem.hpp"
#include "../../Resources/Texture.hpp" // Existing Texture class
#include "../../Resources/Model.hpp"   // Existing Model class
#include "../Logger.hpp"

namespace Cogent::Resources {

    class ResourceManager {
    public:
        static ResourceManager& Get() {
            static ResourceManager instance;
            return instance;
        }

        // Initialize with GPU pointers needed for loading
        void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, Cogent::Resources::Streamer* streamerRef) {
            _device = device;
            _physicalDevice = physicalDevice;
            _commandPool = commandPool;
            _queue = queue;
            _streamer = streamerRef;
        }

        // Request a texture (Async via Streamer)
        std::shared_ptr<Texture> GetTexture(const std::string& path) {
            std::lock_guard<std::mutex> lock(_mutex);
            
            if (_textures.find(path) != _textures.end()) {
                return _textures[path];
            }

            // Create new texture resource
            auto texture = std::make_shared<Texture>();
            texture->path = path;
            
            _textures[path] = texture;

            // Register with Streamer and Request Load
            if (_streamer) {
                _streamer->registerResource(texture);
                _streamer->requestLoad(texture);
            } else {
                LOG_ERROR("Streamer not initialized in ResourceManager! Performing synchronous load.");
                texture->load(_device, _physicalDevice, _commandPool, _queue, path);
            }

            return texture;
        }

        void UpdateStreamer(const glm::vec3& cameraPos, float deltaTime) {
            if (_streamer) {
                _streamer->update(cameraPos, deltaTime);
            }
        }

    private:
        ResourceManager() {}
        
        VkDevice _device;
        VkPhysicalDevice _physicalDevice;
        VkCommandPool _commandPool;
        VkQueue _queue;
        Cogent::Resources::Streamer* _streamer = nullptr;

        std::unordered_map<std::string, std::shared_ptr<Texture>> _textures;
        std::mutex _mutex;
        std::mutex _queueMutex; // Protect Vulkan Queue submission
    };
}
