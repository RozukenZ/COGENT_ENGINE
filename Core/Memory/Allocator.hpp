#pragma once
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace Cogent::Memory {

    class Allocator {
    public:
        Allocator(size_t size, void* start)
            : _size(size), _start(start), _used_memory(0), _num_allocations(0) {}

        virtual ~Allocator() {
            _start = nullptr;
            _size = 0;
        }

        virtual void* allocate(size_t size, uint8_t alignment = 8) = 0;
        virtual void deallocate(void* p) = 0;
        virtual void clear() = 0;

        size_t getSize() const { return _size; }
        size_t getUsedMemory() const { return _used_memory; }
        size_t getNumAllocations() const { return _num_allocations; }

    protected:
        void* _start;
        size_t _size;
        size_t _used_memory;
        size_t _num_allocations;
    };

    inline void* alignForward(void* address, uint8_t alignment) {
        uintptr_t scalarAddress = reinterpret_cast<uintptr_t>(address);
        uintptr_t alignedAddress = (scalarAddress + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
        return reinterpret_cast<void*>(alignedAddress);
    }
}
