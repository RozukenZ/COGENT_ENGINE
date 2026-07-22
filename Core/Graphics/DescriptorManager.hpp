#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>

class DescriptorAllocator {
public:
    struct PoolSizes {
        std::vector<std::pair<VkDescriptorType, float>> sizes =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
        };
    };

    void init(VkDevice device);
    void cleanup();
    void resetPools();
    bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

private:
    VkDescriptorPool createPool(uint32_t count, VkDescriptorPoolCreateFlags flags);
    VkDescriptorPool grabPool();

    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool currentPool = VK_NULL_HANDLE;
    PoolSizes descriptorSizes;
    std::vector<VkDescriptorPool> usedPools;
    std::vector<VkDescriptorPool> freePools;
};

class DescriptorLayoutCache {
public:
    void init(VkDevice device);
    void cleanup();
    VkDescriptorSetLayout createDescriptorLayout(VkDescriptorSetLayoutCreateInfo* info);
    VkDevice getDevice() const { return device; }

private:
    struct DescriptorLayoutInfo {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bool operator==(const DescriptorLayoutInfo& other) const;
        size_t hash() const;
    };

    struct DescriptorLayoutHash {
        size_t operator()(const DescriptorLayoutInfo& k) const {
            return k.hash();
        }
    };

    VkDevice device = VK_NULL_HANDLE;
    std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> layoutCache;
};

class DescriptorBuilder {
public:
    static DescriptorBuilder begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator);
    DescriptorBuilder& bindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
    DescriptorBuilder& bindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
    bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
    bool build(VkDescriptorSet& set);

private:
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    DescriptorLayoutCache* cache = nullptr;
    DescriptorAllocator* alloc = nullptr;
};
