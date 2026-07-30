#pragma once
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <iostream>

namespace Cogent::Threading {

    // A lightweight Job Handle to track job completion
    struct JobHandle {
        std::shared_ptr<std::atomic<bool>> isFinished;
        
        bool IsCompleted() const {
            return isFinished && isFinished->load(std::memory_order_relaxed);
        }
        
        void Wait() const {
            if (!isFinished) return;
            while (!isFinished->load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    };

    class JobSystem {
    public:
        using Job = std::function<void()>;

        static JobSystem& Get() {
            static JobSystem instance;
            return instance;
        }

        void Initialize() {
            if (_initialized) return;

            unsigned int numCores = std::thread::hardware_concurrency();
            // Leave one core for the main thread, ensure at least 1 worker
            unsigned int numWorkers = (numCores > 1) ? numCores - 1 : 1; 

            _shutDown.store(false, std::memory_order_release);
            _activeJobs.store(0, std::memory_order_release);
            _totalJobsExecuted.store(0, std::memory_order_relaxed);

            for (unsigned int i = 0; i < numWorkers; ++i) {
                _workerThreads.emplace_back([this, i] {
                    this->WorkerLoop(i);
                });
            }
            _initialized = true;
            std::cout << "[CTS] Initialized with " << numWorkers << " worker threads.\n";
        }

        JobHandle Execute(const Job& job) {
            assert(_initialized && "JobSystem is not initialized!");
            
            auto handleState = std::make_shared<std::atomic<bool>>(false);
            JobHandle handle{ handleState };
            
            _activeJobs.fetch_add(1, std::memory_order_release);

            Job wrappedJob = [job, handleState, this]() {
                // Future: Insert PROFILE_SCOPE("WorkerExecution") here for NX Insight
                job();
                handleState->store(true, std::memory_order_release);
                this->_activeJobs.fetch_sub(1, std::memory_order_release);
                this->_totalJobsExecuted.fetch_add(1, std::memory_order_relaxed);
            };

            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                _jobQueue.push(std::move(wrappedJob));
            }
            
            _condition.notify_one();
            return handle;
        }

        bool IsBusy() const {
            return _activeJobs.load(std::memory_order_acquire) > 0;
        }

        void WaitAll() const {
            while (IsBusy()) {
                std::this_thread::yield();
            }
        }

        void Shutdown() {
            if (!_initialized) return;
            
            _shutDown.store(true, std::memory_order_release);
            _condition.notify_all();
            
            for (std::thread& worker : _workerThreads) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            _workerThreads.clear();
            _initialized = false;
        }

        uint64_t GetTotalJobsExecuted() const {
            return _totalJobsExecuted.load(std::memory_order_relaxed);
        }

    private:
        JobSystem() : _initialized(false), _shutDown(false), _activeJobs(0), _totalJobsExecuted(0) {}
        ~JobSystem() { Shutdown(); }

        void WorkerLoop(unsigned int threadIndex) {
            (void)threadIndex; // Profiler could use threadIndex to name the thread
            
            while (true) {
                Job job;
                {
                    std::unique_lock<std::mutex> lock(_queueMutex);
                    _condition.wait(lock, [this] { 
                        return _shutDown.load(std::memory_order_acquire) || !_jobQueue.empty(); 
                    });

                    if (_shutDown.load(std::memory_order_acquire) && _jobQueue.empty()) {
                        return; // Thread exit
                    }

                    job = std::move(_jobQueue.front());
                    _jobQueue.pop();
                }
                
                // Execute job outside the lock
                if (job) {
                    job();
                }
            }
        }

        std::vector<std::thread> _workerThreads;
        std::queue<Job> _jobQueue;
        std::mutex _queueMutex;
        std::condition_variable _condition;
        
        bool _initialized;
        std::atomic<bool> _shutDown;
        std::atomic<uint64_t> _activeJobs;
        std::atomic<uint64_t> _totalJobsExecuted; // Telemetry
    };
}
