#pragma once
#include "Allocator.hpp"
#include <mutex>
#include <cassert>

namespace Cogent::Memory {

    class PoolAllocator : public Allocator {
    public:
        PoolAllocator(size_t objectSize, uint8_t objectAlignment, size_t size, void* start, const std::string& name = "PoolAllocator")
            : Allocator(size, start, name), _objectSize(objectSize), _objectAlignment(objectAlignment) {
            
            // Ensure object size is at least size of a pointer for free list
            if (_objectSize < sizeof(void*)) _objectSize = sizeof(void*);
            
            _free_list = nullptr;
            initializeFreeList();
        }

        ~PoolAllocator() override {
            _free_list = nullptr;
        }

        void* allocate(size_t size, uint8_t alignment = 8) override {
            assert(size == _objectSize && "PoolAllocator only allocates objects of fixed size!");
            (void)size; // silence unused warning in release
            (void)alignment; // alignment is handled during initialization

            std::lock_guard<std::mutex> lock(_mutex);
            
            if (_free_list == nullptr) return nullptr; // Out of memory

            void* p = _free_list;
            _free_list = *static_cast<void**>(_free_list);
            
            _used_memory.fetch_add(_objectSize, std::memory_order_relaxed);
            _num_allocations.fetch_add(1, std::memory_order_relaxed);
            
            return p;
        }

        void deallocate(void* p) override {
            if (!p) return;
            
            std::lock_guard<std::mutex> lock(_mutex);
            
            *static_cast<void**>(p) = _free_list;
            _free_list = p;
            
            _used_memory.fetch_sub(_objectSize, std::memory_order_relaxed);
            _num_allocations.fetch_sub(1, std::memory_order_relaxed);
        }

        void clear() override {
            std::lock_guard<std::mutex> lock(_mutex);
            initializeFreeList();
            _used_memory.store(0, std::memory_order_relaxed);
            _num_allocations.store(0, std::memory_order_relaxed);
        }

    private:
        void initializeFreeList() {
            void* p = _start;
            void* aligned_p = alignForward(p, _objectAlignment);
            size_t adjustment = reinterpret_cast<uintptr_t>(aligned_p) - reinterpret_cast<uintptr_t>(p);
            
            _free_list = aligned_p;
            
            size_t numObjects = (_size - adjustment) / _objectSize;
            
            if (numObjects == 0) {
                _free_list = nullptr;
                return;
            }

            void** curr = static_cast<void**>(_free_list);
            for (size_t i = 0; i < numObjects - 1; ++i) {
                void* next = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(curr) + _objectSize);
                *curr = next;
                curr = static_cast<void**>(next);
            }
            *curr = nullptr;
        }

        size_t _objectSize;
        uint8_t _objectAlignment;
        void* _free_list;
        std::mutex _mutex; // Thread-safe protection for concurrent entity/job allocation
    };
}
