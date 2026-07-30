#pragma once
#include "Allocator.hpp"
#include <mutex>
#include <cassert>

namespace Cogent::Memory {

    class StackAllocator : public Allocator {
    public:
        typedef size_t Marker;

        StackAllocator(size_t size, void* start, const std::string& name = "StackAllocator") 
            : Allocator(size, start, name), _current_pos(start) {}

        ~StackAllocator() override {
            clear();
        }

        void* allocate(size_t size, uint8_t alignment = 8) override {
            std::lock_guard<std::mutex> lock(_mutex);
            
            void* p = _current_pos;
            void* aligned_p = alignForward(p, alignment);
            
            size_t adjustment = reinterpret_cast<uintptr_t>(aligned_p) - reinterpret_cast<uintptr_t>(p);
            
            // In a real stack allocator we might want to store the adjustment header,
            // but for simplicity we rely on the Marker to rollback.
            size_t total_size = size + adjustment;

            if (_used_memory.load(std::memory_order_relaxed) + total_size > _size) {
                 return nullptr;
            }

            _used_memory.fetch_add(total_size, std::memory_order_relaxed);
            _num_allocations.fetch_add(1, std::memory_order_relaxed);
            
            _current_pos = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(aligned_p) + size);

            return aligned_p;
        }

        void deallocate(void* p) override {
            assert(false && "Use freeToMarker() for StackAllocator, individual deallocation is not supported.");
            (void)p;
        }

        Marker getMarker() {
            std::lock_guard<std::mutex> lock(_mutex);
            return reinterpret_cast<uintptr_t>(_current_pos) - reinterpret_cast<uintptr_t>(_start);
        }

        void freeToMarker(Marker marker) {
            std::lock_guard<std::mutex> lock(_mutex);
            void* target_pos = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(_start) + marker);
            
            assert(reinterpret_cast<uintptr_t>(target_pos) <= reinterpret_cast<uintptr_t>(_current_pos) && "Invalid marker position");
            
            size_t freed_memory = reinterpret_cast<uintptr_t>(_current_pos) - reinterpret_cast<uintptr_t>(target_pos);
            _used_memory.fetch_sub(freed_memory, std::memory_order_relaxed);
            
            // Approximation for telemetry. We don't accurately track allocation count down without headers.
            // In a fully robust engine, headers are stored.
            
            _current_pos = target_pos;
        }

        void clear() override {
            std::lock_guard<std::mutex> lock(_mutex);
            _current_pos = _start;
            _used_memory.store(0, std::memory_order_relaxed);
            _num_allocations.store(0, std::memory_order_relaxed);
        }

    private:
        void* _current_pos;
        std::mutex _mutex;
    };
}
