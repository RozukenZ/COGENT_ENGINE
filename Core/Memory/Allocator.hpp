#pragma once
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <string>

namespace Cogent::Memory {

    class Allocator {
    public:
        Allocator(size_t size, void* start, const std::string& name = "Unknown")
            : _size(size), _start(start), _used_memory(0), _num_allocations(0), _name(name) {}

        virtual ~Allocator() {
            _start = nullptr;
            _size = 0;
        }

        virtual void* allocate(size_t size, uint8_t alignment = 8) = 0;
        virtual void deallocate(void* p) = 0;
        virtual void clear() = 0;

        size_t getSize() const { return _size; }
        size_t getUsedMemory() const { return _used_memory.load(std::memory_order_relaxed); }
        size_t getNumAllocations() const { return _num_allocations.load(std::memory_order_relaxed); }
        const std::string& getName() const { return _name; }

    protected:
        void* _start;
        size_t _size;
        std::atomic<size_t> _used_memory;
        std::atomic<size_t> _num_allocations;
        std::string _name;
    };

    inline void* alignForward(void* address, uint8_t alignment) {
        uintptr_t scalarAddress = reinterpret_cast<uintptr_t>(address);
        uintptr_t alignedAddress = (scalarAddress + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
        return reinterpret_cast<void*>(alignedAddress);
    }
}
