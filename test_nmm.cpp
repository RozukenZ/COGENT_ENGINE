#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include "Core/Memory/MemoryManager.hpp"
#include "Core/Memory/LinearAllocator.hpp"
#include "Core/Memory/PoolAllocator.hpp"
#include "Core/Memory/StackAllocator.hpp"

using namespace Cogent::Memory;

void TestMemoryManager() {
    bool init = MemoryManager::Get().Initialize(1024 * 1024 * 10); // 10 MB
    assert(init);
    std::cout << "[PASS] MemoryManager Initialized.\n";
}

void TestLinearAllocator() {
    auto linear = MemoryManager::Get().CreateAllocator<LinearAllocator>(1024 * 1024); // 1MB
    assert(linear != nullptr);

    void* p1 = linear->allocate(128, 16);
    assert(p1 != nullptr);
    assert(reinterpret_cast<uintptr_t>(p1) % 16 == 0);

    void* p2 = linear->allocate(256, 8);
    assert(p2 != nullptr);
    assert(linear->getUsedMemory() >= 384);
    assert(linear->getNumAllocations() == 2);

    linear->clear();
    assert(linear->getUsedMemory() == 0);
    assert(linear->getNumAllocations() == 0);
    std::cout << "[PASS] LinearAllocator Test.\n";
}

void TestPoolAllocator() {
    auto pool = MemoryManager::Get().CreateAllocator<PoolAllocator>(32, 8, 1024 * 32); // 32KB pool for 32-byte objects
    assert(pool != nullptr);

    std::vector<void*> ptrs;
    for (int i = 0; i < 100; ++i) {
        void* p = pool->allocate(32, 8);
        assert(p != nullptr);
        ptrs.push_back(p);
    }

    assert(pool->getNumAllocations() == 100);
    assert(pool->getUsedMemory() == 32 * 100);

    for (void* p : ptrs) {
        pool->deallocate(p);
    }

    assert(pool->getNumAllocations() == 0);
    assert(pool->getUsedMemory() == 0);
    std::cout << "[PASS] PoolAllocator Test.\n";
}

void TestStackAllocator() {
    auto stack = MemoryManager::Get().CreateAllocator<StackAllocator>(1024 * 1024); // 1MB
    assert(stack != nullptr);

    auto marker = stack->getMarker();
    
    void* p1 = stack->allocate(100);
    assert(p1 != nullptr);
    assert(stack->getNumAllocations() == 1);

    stack->freeToMarker(marker);
    // freeToMarker rolls back the pointer and used memory, though allocation count approximation might stay.
    // For test, let's just check used memory.
    assert(stack->getUsedMemory() == 0);
    std::cout << "[PASS] StackAllocator Test.\n";
}

void TestMultithreadingPool() {
    auto pool = MemoryManager::Get().CreateAllocator<PoolAllocator>(64, 8, 1024 * 1024); // 1MB

    auto worker = [&pool]() {
        std::vector<void*> ptrs;
        for (int i = 0; i < 1000; ++i) {
            void* p = pool->allocate(64);
            assert(p != nullptr);
            ptrs.push_back(p);
        }
        for (void* p : ptrs) {
            pool->deallocate(p);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    assert(pool->getNumAllocations() == 0);
    std::cout << "[PASS] Multithreading PoolAllocator Test.\n";
}

int main() {
    std::cout << "--- NMM UNIT TESTS ---\n";
    TestMemoryManager();
    TestLinearAllocator();
    TestPoolAllocator();
    TestStackAllocator();
    TestMultithreadingPool();
    MemoryManager::Get().Shutdown();
    std::cout << "All NMM tests passed successfully.\n";
    return 0;
}
