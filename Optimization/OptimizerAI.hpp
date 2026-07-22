#pragma once
#include "SceneAnalyzer.hpp"
#include <iostream>
#include <algorithm>

namespace Cogent::Optimization {

    enum class PerformanceMode {
        RAW,        // Minimal intervention
        BALANCED,   // Target 60 FPS
        PERFORMANCE,// Target 120 FPS / Aggr. Culling
        LOW_POWER   // Cap at 30 FPS
    };

    struct RenderSettings {
        float renderScale = 1.0f;
        int shadowQuality = 2; // 0=Low, 1=Med, 2=High
        float lodBias = 0.0f;
        int taaSamples = 4;
    };

    class OptimizerAI {
    public:
        OptimizerAI() = default;

        void update(float deltaTime) {
            timer += deltaTime;
            if (timer < checkInterval) return;
            timer = 0.0f;

            analyze();
        }

        void setMode(PerformanceMode newMode) { 
            mode = newMode; 
            applyPreset(mode);
        }

        const RenderSettings& getSettings() const { return currentSettings; }

    private:
        void analyze() {
            const auto& stats = SceneAnalyzer::Get().getStats();
            float targetFrameTime = 16.6f; // 60 FPS default

            if (mode == PerformanceMode::PERFORMANCE) targetFrameTime = 8.3f; // 120 FPS
            if (mode == PerformanceMode::LOW_POWER) targetFrameTime = 33.3f; // 30 FPS

            // Simple Heuristic: Hysteresis
            if (stats.frameTime > targetFrameTime * 1.1f) {
                // Drop Quality
                reduceQuality();
            } else if (stats.frameTime < targetFrameTime * 0.8f) {
                // Increase Quality (Slowly)
                increaseQuality();
            }
        }

        void reduceQuality() {
            if (currentSettings.renderScale > 0.5f) {
                currentSettings.renderScale -= 0.1f;
                // std::cout << "[OptimizerAI] Reducing Render Scale to " << currentSettings.renderScale << std::endl;
            }
            if (currentSettings.shadowQuality > 0) {
                 currentSettings.shadowQuality--;
            }
        }

        void increaseQuality() {
             if (currentSettings.renderScale < 1.0f) {
                 currentSettings.renderScale += 0.05f;
             }
        }

        void applyPreset(PerformanceMode m) {
            switch (m) {
                case PerformanceMode::RAW: 
                    currentSettings = {1.0f, 2, 0.0f, 8}; break;
                case PerformanceMode::BALANCED: 
                    currentSettings = {1.0f, 1, 0.0f, 4}; break;
                // ...
            }
        }

        PerformanceMode mode = PerformanceMode::BALANCED;
        RenderSettings currentSettings;
        float timer = 0.0f;
        float checkInterval = 1.0f; // Check every 1 second
    };
}
