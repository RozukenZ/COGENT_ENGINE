#include <iostream>
#include <cassert>
#include <atomic>
#include <vector>
#include "Core/Threading/JobSystem.hpp"

using namespace Cogent::Threading;

void TestSingleJob() {
    std::atomic<int> counter = 0;
    
    auto handle = JobSystem::Get().Execute([&counter]() {
        for (int i = 0; i < 1000; i++) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    });

    handle.Wait();
    assert(handle.IsCompleted());
    assert(counter.load() == 1000);
    std::cout << "[PASS] Single Job Test.\n";
}

void TestConcurrentJobs() {
    std::atomic<int> counter = 0;
    std::vector<JobHandle> handles;
    
    const int numJobs = 10000;
    for (int i = 0; i < numJobs; i++) {
        handles.push_back(JobSystem::Get().Execute([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    JobSystem::Get().WaitAll();
    
    assert(counter.load() == numJobs);
    std::cout << "[PASS] Concurrent Jobs Test (10,000 jobs).\n";
}

void TestWaitAll() {
    std::atomic<int> counter = 0;
    for (int i = 0; i < 50; i++) {
        JobSystem::Get().Execute([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    JobSystem::Get().WaitAll();
    assert(counter.load() == 50);
    std::cout << "[PASS] WaitAll Synchronization Test.\n";
}

int main() {
    std::cout << "--- CTS UNIT TESTS ---\n";
    JobSystem::Get().Initialize();
    
    TestSingleJob();
    TestConcurrentJobs();
    TestWaitAll();
    
    std::cout << "Total jobs executed across all tests: " << JobSystem::Get().GetTotalJobsExecuted() << "\n";
    
    JobSystem::Get().Shutdown();
    std::cout << "All CTS tests passed successfully.\n";
    return 0;
}
