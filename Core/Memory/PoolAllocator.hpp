#pragma once
#include "Allocator.hpp"
#include <vector>

namespace Cogent::Memory {

    class PoolAllocator : public Allocator {
    public:
        PoolAllocator(size_t objectSize, uint8_t objectAlignment, size_t size, void* start)
            : Allocator(size, start), _objectSize(objectSize), _objectAlignment(objectAlignment) {
            
            // Ensure object size is at least size of a pointer for free list
            if (_objectSize < sizeof(void*)) _objectSize = sizeof(void*);
            
            // Calculate adjustment needed to align the first object
            void* p = _start;
            void* aligned_p = alignForward(p, _objectAlignment);
            size_t adjustment = reinterpret_cast<uintptr_t>(aligned_p) - reinterpret_cast<uintptr_t>(p);
            
            _free_list = aligned_p;
            
            size_t numObjects = (size - adjustment) / _objectSize;
            
            void** curr = static_cast<void**>(_free_list);
            for (size_t i = 0; i < numObjects - 1; ++i) {
                *curr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(curr) + _objectSize);
                curr = static_cast<void**>(*curr);
            }
            *curr = nullptr;
        }

        ~PoolAllocator() override {
            _free_list = nullptr;
        }

        void* allocate(size_t size, uint8_t alignment = 8) override {
            if (size != _objectSize && size != 0) return nullptr; // Can only allocate fixed size
            
            if (_free_list == nullptr) return nullptr; // Out of memory

            void* p = _free_list;
            _free_list = *static_cast<void**>(_free_list);
            
            _used_memory += _objectSize;
            _num_allocations++;
            
            return p;
        }

        void deallocate(void* p) override {
            *static_cast<void**>(p) = _free_list;
            _free_list = p;
            
            _used_memory -= _objectSize;
            _num_allocations--;
        }

        void clear() override {
             // Re-initialize free list? 
             // Ideally we shouldn't clear a pool often, better to reconstruct.
             // For now, this is a placeholder.
        }

    private:
        size_t _objectSize;
        uint8_t _objectAlignment;
        void* _free_list;
    };
}
