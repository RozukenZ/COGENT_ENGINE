#include "InstanceBuffer.hpp"
#include <cstring>
#include <stdexcept>

namespace Cogent::Renderer {

    InstanceBuffer::InstanceBuffer(GraphicsDevice& device) : device(device) {}

    InstanceBuffer::~InstanceBuffer() {
        if (buffer) {
            vkDestroyBuffer(device.getDevice(), buffer, nullptr);
            vkFreeMemory(device.getDevice(), memory, nullptr);
        }
    }

    void InstanceBuffer::update(const std::vector<InstanceData>& instances) {
        if (instances.empty()) return;

        VkDeviceSize newSize = sizeof(InstanceData) * instances.size();
        instanceCount = static_cast<uint32_t>(instances.size());

        // Reallocate if too small
        if (newSize > bufferSize) {
            if (buffer) {
                vkDestroyBuffer(device.getDevice(), buffer, nullptr);
                vkFreeMemory(device.getDevice(), memory, nullptr);
            }

            bufferSize = newSize;

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; 
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device.getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create Instance Buffer!");
            }

            VkMemoryRequirements memReqs;
            vkGetBufferMemoryRequirements(device.getDevice(), buffer, &memReqs);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memReqs.size;
            // Host Visible for frequent updates (Phase 2 optimization: Use Staging Buffer + Device Local)
            allocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate Instance Memory!");
            }
            vkBindBufferMemory(device.getDevice(), buffer, memory, 0);
        }

        // Map and Copy
        void* data;
        vkMapMemory(device.getDevice(), memory, 0, newSize, 0, &data);
        memcpy(data, instances.data(), (size_t)newSize);
        vkUnmapMemory(device.getDevice(), memory);
    }
}
