#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <future>
#include <mutex>
#include <shared_mutex>
#include "../Core/Threading/JobSystem.hpp"
#include "Texture.hpp" 
#include "Model.hpp"   
#include "../Core/Logger.hpp"

namespace Cogent::Resources {

    // Advanced Resource Manager with Read-Write locks and proper Garbage Collection
    class ResourceManager {
    public:
        static ResourceManager& Get() {
            static ResourceManager instance;
            return instance;
        }

        void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, Cogent::Resources::Streamer* streamerRef) {
            _device = device;
            _physicalDevice = physicalDevice;
            _commandPool = commandPool;
            _queue = queue;
            _streamer = streamerRef;
            LOG_INFO("ResourceManager initialized.");
        }

        // Get or Create Texture mapping
        std::shared_ptr<Texture> GetTexture(const std::string& path) {
            {
                std::shared_lock<std::shared_mutex> readLock(_rwMutex);
                auto it = _textures.find(path);
                if (it != _textures.end()) {
                    return it->second;
                }
            }

            // Not found, upgrade to unique lock
            std::unique_lock<std::shared_mutex> writeLock(_rwMutex);
            
            // Double check in case another thread created it
            auto it = _textures.find(path);
            if (it != _textures.end()) {
                return it->second;
            }

            auto texture = std::make_shared<Texture>();
            texture->path = path;
            _textures[path] = texture;

            if (_streamer) {
                _streamer->registerResource(texture);
                _streamer->requestLoad(texture);
            } else {
                LOG_ERROR("Streamer offline. Falling back to synchronous texture load.");
                texture->load(_device, _physicalDevice, _commandPool, _queue, path);
            }

            return texture;
        }

        void UpdateStreamer(const glm::vec3& cameraPos, float deltaTime) {
            if (_streamer) {
                _streamer->update(cameraPos, deltaTime);
            }
        }

        // Garbage Collector: Unloads resources that have use_count == 1 (Only owned by Manager)
        void UnloadUnused() {
            std::unique_lock<std::shared_mutex> writeLock(_rwMutex);
            
            size_t beforeCount = _textures.size();
            
            for (auto it = _textures.begin(); it != _textures.end(); ) {
                if (it->second.use_count() == 1) {
                    LOG_INFO("Unloading unused resource: " + it->first);
                    it->second->unload(); 
                    it = _textures.erase(it);
                } else {
                    ++it;
                }
            }
            
            if (_textures.size() < beforeCount) {
                 LOG_INFO("Garbage Collection complete. Freed " + std::to_string(beforeCount - _textures.size()) + " resources.");
            }
        }
        
        size_t GetActiveResourceCount() {
            std::shared_lock<std::shared_mutex> readLock(_rwMutex);
            return _textures.size();
        }

    private:
        ResourceManager() = default;
        ~ResourceManager() = default;
        
        VkDevice _device = VK_NULL_HANDLE;
        VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
        VkCommandPool _commandPool = VK_NULL_HANDLE;
        VkQueue _queue = VK_NULL_HANDLE;
        Cogent::Resources::Streamer* _streamer = nullptr;

        std::unordered_map<std::string, std::shared_ptr<Texture>> _textures;
        
        // C++17 shared_mutex for high performance concurrent reads
        std::shared_mutex _rwMutex; 
    };
}
