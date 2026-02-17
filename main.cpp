#include <iostream>
#include <stdexcept>
#include "Engine/CogentEngine.hpp"
#include "Core/Logger.hpp"

int main() {
    // 1. Init Logging
    // Use relative path to build directory or where executable runs
    LOG_INIT("cogent_log.txt");
    LOG_INFO("COGENT ENGINE STARTED");
    LOG_INFO("Bootstrapping System...");

    CogentEngine app;

    try {
        app.run();
    } catch (const std::exception& e) {
        LOG_ERROR("FATAL ERROR: " + std::string(e.what()));
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    LOG_INFO("COGENT ENGINE SHUTDOWN GRACEFULLY");
    return EXIT_SUCCESS;
}