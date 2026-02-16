#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <filesystem>

// Simple thread-safe logger
class Logger {
public:
    enum class Level {
        INFO,
        WARN,
        ERROR_LOG
    };

    static Logger& Get() {
        static Logger instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Init(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_File.open(filename, std::ios::out | std::ios::trunc);
        if (!m_File.is_open()) {
            std::cerr << "[Logger] Failed to open log file: " << filename << std::endl;
        } else {
             LogInternal(Level::INFO, "Logger initialized. Writing to " + filename);
        }
    }

    void Log(Level level, const std::string& message, const char* file, int line) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        LogInternal(level, message, file, line);
    }

private:
    Logger() {}
    
    ~Logger() {
        if (m_File.is_open()) {
            m_File.close();
        }
    }

    void LogInternal(Level level, const std::string& message, const char* file = nullptr, int line = 0) {
        // Timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm;
        #ifdef _WIN32
            localtime_s(&local_tm, &now_time);
        #else
            localtime_r(&now_time, &local_tm);
        #endif

        char timeStr[20];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &local_tm);

        // Level String
        std::string levelStr;
        std::string colorCode;
        switch (level) {
            case Level::INFO:      levelStr = "[INFO] "; colorCode = "\033[32m"; break; // Green
            case Level::WARN:      levelStr = "[WARN] "; colorCode = "\033[33m"; break; // Yellow
            case Level::ERROR_LOG: levelStr = "[ERROR]"; colorCode = "\033[31m"; break; // Red
        }

        std::stringstream ss;
        ss << "[" << timeStr << "] " << levelStr << " " << message;
        
        if (file) {
            std::filesystem::path p(file);
            ss << " (" << p.filename().string() << ":" << line << ")";
        }

        std::string finalLog = ss.str();

        // Print to Console
        std::cout << colorCode << finalLog << "\033[0m" << std::endl;

        // Print to File
        if (m_File.is_open()) {
            m_File << finalLog << std::endl;
            m_File.flush(); // Ensure it's written immediately
        }
    }

    std::ofstream m_File;
    std::mutex m_Mutex;
};

// Macros for easy logging
#define LOG_INIT(filename) Logger::Get().Init(filename)
#define LOG_INFO(msg)      Logger::Get().Log(Logger::Level::INFO, msg, __FILE__, __LINE__)
#define LOG_WARN(msg)      Logger::Get().Log(Logger::Level::WARN, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg)     Logger::Get().Log(Logger::Level::ERROR_LOG, msg, __FILE__, __LINE__)
