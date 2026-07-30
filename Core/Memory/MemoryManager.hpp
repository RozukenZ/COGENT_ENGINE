#pragma once
#include "Allocator.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace Cogent::Memory {

    class MemoryManager {
    public:
        // Singleton access
        static MemoryManager& Get();

        // Initialize the global memory block
        bool Initialize(size_t globalMemorySize);
        void Shutdown();

        // Vend allocators dynamically out of the main block
        template<typename T, typename... Args>
        T* CreateAllocator(size_t size, Args&&... args) {
            std::lock_guard<std::mutex> lock(_mutex);
            
            if (_used_memory + size > _total_memory) {
                return nullptr; // Out of global memory
            }

            void* allocator_start = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(_global_memory) + _used_memory);
            
            // Placement new the allocator class into our tracking structure or heap (allocator instance is small, block is large)
            // For simplicity, the allocator metadata object goes to heap, but the block it manages is from our global block.
            T* new_allocator = new T(size, allocator_start, std::forward<Args>(args)...);
            
            _allocators.push_back(new_allocator);
            _used_memory += size;

            return new_allocator;
        }

        void DestroyAllocator(Allocator* allocator);

        size_t GetTotalMemory() const { return _total_memory; }
        size_t GetUsedGlobalMemory() const { return _used_memory; }
        
        // Aggregates telemetry from all active allocators
        size_t GetTotalAllocatedObjectsMemory();

    private:
        MemoryManager() = default;
        ~MemoryManager() = default;

        MemoryManager(const MemoryManager&) = delete;
        MemoryManager& operator=(const MemoryManager&) = delete;

        void* _global_memory = nullptr;
        size_t _total_memory = 0;
        size_t _used_memory = 0;

        std::vector<Allocator*> _allocators;
        std::mutex _mutex;
    };
}
