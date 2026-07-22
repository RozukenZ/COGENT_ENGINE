#pragma once
#include <string>
#include <chrono>
#include <unordered_map>
#include <iostream>
#include <mutex>

namespace Cogent::Diagnostics {

    struct ProfileResult {
        std::string name;
        long long duration; // Microseconds
    };

    class Profiler {
    public:
        static Profiler& Get() {
            static Profiler instance;
            return instance;
        }

        void BeginSession(const std::string& name) {
            // Optional: Open file for chrome tracing format
        }

        void EndSession() {
        }

        void WriteProfile(const ProfileResult& result) {
            std::lock_guard<std::mutex> lock(_mutex);
            // Just print to console for now, or store for UI
            // std::cout << "Profile [" << result.name << "]: " << result.duration << "us" << std::endl;
            _results[result.name] = result.duration;
        }

        const std::unordered_map<std::string, long long>& GetResults() { return _results; }

    private:
         std::unordered_map<std::string, long long> _results;
         std::mutex _mutex;
    };

    class InstrumentationTimer {
    public:
        InstrumentationTimer(const char* name) : _name(name), _startTimepoint(std::chrono::high_resolution_clock::now()) {}

        ~InstrumentationTimer() {
            if (!_stopped) Stop();
        }

        void Stop() {
            auto endTimepoint = std::chrono::high_resolution_clock::now();
            auto start = std::chrono::time_point_cast<std::chrono::microseconds>(_startTimepoint).time_since_epoch().count();
            auto end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

            auto duration = end - start;
            Profiler::Get().WriteProfile({ _name, duration });
            _stopped = true;
        }

    private:
        const char* _name;
        std::chrono::time_point<std::chrono::high_resolution_clock> _startTimepoint;
        bool _stopped = false;
    };
}

#define PROFILE_SCOPE(name) Cogent::Diagnostics::InstrumentationTimer timer##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
