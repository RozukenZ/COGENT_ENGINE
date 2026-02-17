#pragma once
#include "Allocator.hpp"

namespace Cogent::Memory {

    class LinearAllocator : public Allocator {
    public:
        LinearAllocator(size_t size, void* start) 
            : Allocator(size, start), _current_pos(start) {}

        ~LinearAllocator() override {
            clear();
        }

        void* allocate(size_t size, uint8_t alignment = 8) override {
            void* p = _current_pos;
            void* aligned_p = alignForward(p, alignment);
            
            size_t adjustment = reinterpret_cast<uintptr_t>(aligned_p) - reinterpret_cast<uintptr_t>(p);
            size_t total_size = size + adjustment;

            if (_used_memory + total_size > _size) {
                 return nullptr; // Out of memory
            }

            _used_memory += total_size;
            _num_allocations++;
            _current_pos = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(aligned_p) + size);

            return aligned_p;
        }

        void deallocate(void* p) override {
            // Linear allocator does not support individual deallocation
            // Use clear() to reset the entire allocator
        }

        void clear() override {
            _current_pos = _start;
            _used_memory = 0;
            _num_allocations = 0;
        }

    private:
        void* _current_pos;
    };
}
