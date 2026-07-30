#pragma once
#include "Allocator.hpp"
#include <mutex>

namespace Cogent::Memory {

    class LinearAllocator : public Allocator {
    public:
        LinearAllocator(size_t size, void* start, const std::string& name = "LinearAllocator") 
            : Allocator(size, start, name), _current_pos(start) {}

        ~LinearAllocator() override {
            clear();
        }

        void* allocate(size_t size, uint8_t alignment = 8) override {
            std::lock_guard<std::mutex> lock(_mutex);
            
            void* p = _current_pos;
            void* aligned_p = alignForward(p, alignment);
            
            size_t adjustment = reinterpret_cast<uintptr_t>(aligned_p) - reinterpret_cast<uintptr_t>(p);
            size_t total_size = size + adjustment;

            if (_used_memory.load(std::memory_order_relaxed) + total_size > _size) {
                 return nullptr; // Out of memory
            }

            _used_memory.fetch_add(total_size, std::memory_order_relaxed);
            _num_allocations.fetch_add(1, std::memory_order_relaxed);
            
            _current_pos = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(aligned_p) + size);

            return aligned_p;
        }

        void deallocate(void* p) override {
            // Linear allocator does not support individual deallocation
            // Use clear() to reset the entire allocator
            (void)p; 
        }

        void clear() override {
            std::lock_guard<std::mutex> lock(_mutex);
            _current_pos = _start;
            _used_memory.store(0, std::memory_order_relaxed);
            _num_allocations.store(0, std::memory_order_relaxed);
        }

    private:
        void* _current_pos;
        std::mutex _mutex; // Ensures thread-safety for multi-threaded systems (CTS)
    };
}
