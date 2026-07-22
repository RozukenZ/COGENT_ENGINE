#pragma once
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace Cogent::Threading {

    class JobSystem {
    public:
        using Job = std::function<void()>;

        static JobSystem& Get() {
            static JobSystem instance;
            return instance;
        }

        void Initialize() {
            unsigned int numCores = std::thread::hardware_concurrency();
            // Leave one core for the main thread
            if (numCores > 1) numCores--; 

            _shutDown = false;

            for (unsigned int i = 0; i < numCores; ++i) {
                _workerThreads.emplace_back([this] {
                    while (true) {
                        Job job;
                        {
                            std::unique_lock<std::mutex> lock(_queueMutex);
                            _condition.wait(lock, [this] { return _shutDown || !_jobQueue.empty(); });

                            if (_shutDown && _jobQueue.empty()) return;

                            job = std::move(_jobQueue.front());
                            _jobQueue.pop();
                        }
                        job();
                        _finishedLabel.fetch_add(1);
                    }
                });
            }
        }

        void Execute(const Job& job) {
            _currentLabel += 1;
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                _jobQueue.push(job);
            }
            _condition.notify_one();
        }

        bool IsBusy() {
            return _finishedLabel.load() < _currentLabel;
        }

        void Wait() {
            while (IsBusy()) {
                std::this_thread::yield();
            }
        }

        void Shutdown() {
            {
                std::lock_guard<std::mutex> lock(_queueMutex);
                _shutDown = true;
            }
            _condition.notify_all();
            for (std::thread& worker : _workerThreads) {
                if (worker.joinable()) worker.join();
            }
            _workerThreads.clear();
        }

    private:
        JobSystem() : _currentLabel(0), _finishedLabel(0), _shutDown(false) {}
        ~JobSystem() { Shutdown(); }

        std::vector<std::thread> _workerThreads;
        std::queue<Job> _jobQueue;
        std::mutex _queueMutex;
        std::condition_variable _condition;
        
        std::atomic<uint64_t> _currentLabel;
        std::atomic<uint64_t> _finishedLabel;
        bool _shutDown;
    };
}
