#include "MemoryManager.hpp"
#include <cstdlib>
#include <algorithm>

namespace Cogent::Memory {

    MemoryManager& MemoryManager::Get() {
        static MemoryManager instance;
        return instance;
    }

    bool MemoryManager::Initialize(size_t globalMemorySize) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_global_memory != nullptr) return false; // Already initialized

        _global_memory = std::malloc(globalMemorySize);
        if (!_global_memory) {
            return false; // Critical failure
        }

        _total_memory = globalMemorySize;
        _used_memory = 0;
        return true;
    }

    void MemoryManager::Shutdown() {
        std::lock_guard<std::mutex> lock(_mutex);

        for (Allocator* alloc : _allocators) {
            delete alloc;
        }
        _allocators.clear();

        if (_global_memory) {
            std::free(_global_memory);
            _global_memory = nullptr;
        }
        
        _total_memory = 0;
        _used_memory = 0;
    }

    void MemoryManager::DestroyAllocator(Allocator* allocator) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        auto it = std::find(_allocators.begin(), _allocators.end(), allocator);
        if (it != _allocators.end()) {
            delete *it;
            _allocators.erase(it);
            // Note: We don't reclaim _used_memory block fragmentation here.
            // A production manager would need a block allocator like a Buddy Allocator 
            // to reclaim blocks. For Phase 0, linear vending is sufficient.
        }
    }

    size_t MemoryManager::GetTotalAllocatedObjectsMemory() {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t total = 0;
        for (Allocator* alloc : _allocators) {
            total += alloc->getUsedMemory();
        }
        return total;
    }
}
