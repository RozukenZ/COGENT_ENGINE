#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include "../../Core/Graphics/GraphicsDevice.hpp"

namespace Cogent::Renderer {
    struct InstanceData {
        glm::mat4 model;
        glm::vec4 color;
        int id;
        int padding[3];
    };

    class InstanceBuffer {
    public:
        InstanceBuffer(GraphicsDevice& device);
        ~InstanceBuffer();

        void update(const std::vector<InstanceData>& instances);
        VkBuffer getBuffer() const { return buffer; }
        uint32_t getInstanceCount() const { return instanceCount; }

    private:
        GraphicsDevice& device;
        VkBuffer buffer;
        VkDeviceMemory memory;
        uint32_t instanceCount = 0;
        VkDeviceSize bufferSize = 0;
    };
}
