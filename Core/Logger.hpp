#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <vector>
#include <functional>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace Cogent::Core {

    enum class LogLevel {
        INFO,
        WARN,
        ERR, // ERROR is defined in Windows.h
        FATAL
    };

    class Logger {
    public:
        static Logger& Get() {
            static Logger instance;
            return instance;
        }

        void Init(const std::string& filename) {
            std::lock_guard<std::mutex> lock(_mutex);
            _logFile.open(filename, std::ios::out | std::ios::trunc);
            if (!_logFile.is_open()) {
                std::cerr << "Failed to open log file: " << filename << std::endl;
            }
            // Install Crash Handler
            #ifdef _WIN32
            SetUnhandledExceptionFilter(TopLevelExceptionHandler);
            #endif
        }

        template<typename... Args>
        void Log(LogLevel level, Args... args) {
            std::lock_guard<std::mutex> lock(_mutex);
            
            std::stringstream ss;
            ((ss << args), ...);
            
            std::string message = ss.str();
            std::string formatted = FormatMessage(level, message);

            // Print to Console
            if (level == LogLevel::ERR || level == LogLevel::FATAL) {
                std::cerr << formatted << std::endl;
            } else {
                std::cout << formatted << std::endl;
            }

            // Write to File
            if (_logFile.is_open()) {
                _logFile << formatted << std::endl;
                _logFile.flush();
            }

            // Invoke Callbacks (e.g. for Editor Console)
            for (const auto& callback : _callbacks) {
                callback(formatted);
            }

            if (level == LogLevel::FATAL) {
                // Force crash/break
                abort();
            }
        }

        void AddCallback(std::function<void(const std::string&)> callback) {
            std::lock_guard<std::mutex> lock(_mutex);
            _callbacks.push_back(callback);
        }

    private:
        Logger() {}
        ~Logger() {
            if (_logFile.is_open()) _logFile.close();
        }

        std::ofstream _logFile;
        std::mutex _mutex;
        std::vector<std::function<void(const std::string&)>> _callbacks;

        std::string FormatMessage(LogLevel level, const std::string& msg) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            char buf[100];
            std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", std::localtime(&now_c));

            std::string levelStr;
            switch (level) {
                case LogLevel::INFO: levelStr = "[INFO] "; break;
                case LogLevel::WARN: levelStr = "[WARN] "; break;
                case LogLevel::ERR:  levelStr = "[ERROR] "; break;
                case LogLevel::FATAL: levelStr = "[FATAL] "; break;
            }
            return std::string(buf) + " " + levelStr + msg;
        }

        #ifdef _WIN32
        static LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
            Logger::Get().Log(LogLevel::FATAL, "CRASH DETECTED! Exception Code: 0x", std::hex, pExceptionInfo->ExceptionRecord->ExceptionCode);
            Logger::Get().GenerateDump(pExceptionInfo);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        void GenerateDump(PEXCEPTION_POINTERS pExceptionInfo) {
            HANDLE hFile = CreateFileA("start_crash.dmp", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE)) {
                MINIDUMP_EXCEPTION_INFORMATION mdei;
                mdei.ThreadId = GetCurrentThreadId();
                mdei.ExceptionPointers = pExceptionInfo;
                mdei.ClientPointers = FALSE;

                MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mdei, NULL, NULL);
                CloseHandle(hFile);
                Log(LogLevel::ERR, "Minidump created: start_crash.dmp");
            } else {
                Log(LogLevel::ERR, "Failed to create minidump");
            }
        }
        #endif
    };
}

// Global Macros
#define LOG_INIT(file) Cogent::Core::Logger::Get().Init(file)
#define LOG_INFO(...) Cogent::Core::Logger::Get().Log(Cogent::Core::LogLevel::INFO, __VA_ARGS__)
#define LOG_WARN(...) Cogent::Core::Logger::Get().Log(Cogent::Core::LogLevel::WARN, __VA_ARGS__)
#define LOG_ERROR(...) Cogent::Core::Logger::Get().Log(Cogent::Core::LogLevel::ERR, __VA_ARGS__)
#define LOG_FATAL(...) Cogent::Core::Logger::Get().Log(Cogent::Core::LogLevel::FATAL, __VA_ARGS__)
