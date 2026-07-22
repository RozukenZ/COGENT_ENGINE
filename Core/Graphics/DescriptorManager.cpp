#include "DescriptorManager.hpp"
#include <algorithm>

// --- DescriptorAllocator ---
void DescriptorAllocator::init(VkDevice newDevice) {
    device = newDevice;
}

void DescriptorAllocator::cleanup() {
    for (auto p : freePools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    for (auto p : usedPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
}

VkDescriptorPool DescriptorAllocator::createPool(uint32_t count, VkDescriptorPoolCreateFlags flags) {
    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(descriptorSizes.sizes.size());
    for (auto sz : descriptorSizes.sizes) {
        sizes.push_back({ sz.first, uint32_t(sz.second * count) });
    }

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = flags;
    poolInfo.maxSets = count;
    poolInfo.poolSizeCount = (uint32_t)sizes.size();
    poolInfo.pPoolSizes = sizes.data();

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    return descriptorPool;
}

VkDescriptorPool DescriptorAllocator::grabPool() {
    if (freePools.size() > 0) {
        VkDescriptorPool pool = freePools.back();
        freePools.pop_back();
        return pool;
    }
    else {
        return createPool(1000, 0); // create 1000 sets per pool
    }
}

void DescriptorAllocator::resetPools() {
    for (auto p : usedPools) {
        vkResetDescriptorPool(device, p, 0);
        freePools.push_back(p);
    }
    usedPools.clear();
    currentPool = VK_NULL_HANDLE;
}

bool DescriptorAllocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout) {
    if (currentPool == VK_NULL_HANDLE) {
        currentPool = grabPool();
        usedPools.push_back(currentPool);
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = currentPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
    bool needReallocate = false;

    switch (allocResult) {
    case VK_SUCCESS:
        return true;
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        needReallocate = true;
        break;
    default:
        return false;
    }

    if (needReallocate) {
        currentPool = grabPool();
        usedPools.push_back(currentPool);
        allocInfo.descriptorPool = currentPool;
        allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
        if (allocResult == VK_SUCCESS) {
            return true;
        }
    }
    return false;
}

// --- DescriptorLayoutCache ---
void DescriptorLayoutCache::init(VkDevice newDevice) {
    device = newDevice;
}

void DescriptorLayoutCache::cleanup() {
    for (auto pair : layoutCache) {
        vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
    }
}

VkDescriptorSetLayout DescriptorLayoutCache::createDescriptorLayout(VkDescriptorSetLayoutCreateInfo* info) {
    DescriptorLayoutInfo layoutinfo;
    layoutinfo.bindings.reserve(info->bindingCount);
    bool isSorted = true;
    int lastBinding = -1;
    for (uint32_t i = 0; i < info->bindingCount; i++) {
        layoutinfo.bindings.push_back(info->pBindings[i]);
        if ((int)info->pBindings[i].binding > lastBinding) {
            lastBinding = info->pBindings[i].binding;
        }
        else {
            isSorted = false;
        }
    }

    if (!isSorted) {
        std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
            return a.binding < b.binding;
        });
    }

    auto it = layoutCache.find(layoutinfo);
    if (it != layoutCache.end()) {
        return (*it).second;
    }
    else {
        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(device, info, nullptr, &layout);
        layoutCache[layoutinfo] = layout;
        return layout;
    }
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const {
    if (other.bindings.size() != bindings.size()) return false;
    for (int i = 0; i < bindings.size(); i++) {
        if (other.bindings[i].binding != bindings[i].binding) return false;
        if (other.bindings[i].descriptorType != bindings[i].descriptorType) return false;
        if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) return false;
        if (other.bindings[i].stageFlags != bindings[i].stageFlags) return false;
    }
    return true;
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
    size_t result = std::hash<size_t>()(bindings.size());
    for (const VkDescriptorSetLayoutBinding& b : bindings) {
        size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
        result ^= std::hash<size_t>()(binding_hash);
    }
    return result;
}

// --- DescriptorBuilder ---
DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator) {
    DescriptorBuilder builder;
    builder.cache = layoutCache;
    builder.alloc = allocator;
    return builder;
}

DescriptorBuilder& DescriptorBuilder::bindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding newBinding = {};
    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;
    bindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite = {};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.pNext = nullptr;
    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pBufferInfo = bufferInfo;
    newWrite.dstBinding = binding;
    writes.push_back(newWrite);
    return *this;
}

DescriptorBuilder& DescriptorBuilder::bindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding newBinding = {};
    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;
    bindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite = {};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.pNext = nullptr;
    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pImageInfo = imageInfo;
    newWrite.dstBinding = binding;
    writes.push_back(newWrite);
    return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout) {
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.pBindings = bindings.data();
    layoutInfo.bindingCount = (uint32_t)bindings.size();

    layout = cache->createDescriptorLayout(&layoutInfo);
    bool success = alloc->allocate(&set, layout);
    if (!success) return false;

    for (VkWriteDescriptorSet& w : writes) {
        w.dstSet = set;
    }
    vkUpdateDescriptorSets(cache->getDevice(), (uint32_t)writes.size(), writes.data(), 0, nullptr);
    return true;
}

bool DescriptorBuilder::build(VkDescriptorSet& set) {
    VkDescriptorSetLayout layout;
    return build(set, layout);
}
