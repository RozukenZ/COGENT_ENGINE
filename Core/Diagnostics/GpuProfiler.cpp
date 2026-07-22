#include "GpuProfiler.hpp"
#include <iostream>
#include <stdexcept>
#include "../Logger.hpp"

namespace Cogent::Diagnostics {

    void GpuProfiler::Init(GraphicsDevice& device) {
        _device = &device;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device.getPhysicalDevice(), &props);
        _timestampPeriod = props.limits.timestampPeriod;

        VkQueryPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = MAX_QUERIES;

        if (vkCreateQueryPool(device.getDevice(), &poolInfo, nullptr, &_queryPool) != VK_SUCCESS) {
            LOG_ERROR("Failed to create timestamp query pool!");
        }
    }

    void GpuProfiler::Cleanup() {
        if (_queryPool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(_device->getDevice(), _queryPool, nullptr);
        }
    }

    void GpuProfiler::BeginFrame(VkCommandBuffer cmd) {
        vkCmdResetQueryPool(cmd, _queryPool, 0, MAX_QUERIES);
        _currentQuery = 0;
        _activeScopes.clear();
    }

    void GpuProfiler::EndFrame() {
        // Nothing to do here specifically for queries, we wait for next frame or explicit Retrieve
    }

    void GpuProfiler::BeginTimestamp(VkCommandBuffer cmd, const std::string& name) {
        if (_currentQuery >= MAX_QUERIES - 1) return;

        uint32_t idx = _currentQuery++;
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, _queryPool, idx);

        GpuTimerScope scope;
        scope.name = name;
        scope.startIndex = idx;
        scope.endIndex = 0; // Will be set in EndTimestamp
        _activeScopes.push_back(scope);
    }

    void GpuProfiler::EndTimestamp(VkCommandBuffer cmd, const std::string& name) {
        if (_currentQuery >= MAX_QUERIES) return;

        uint32_t idx = _currentQuery++;
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, _queryPool, idx);

        // Find the matching scope
        for (auto& scope : _activeScopes) {
            if (scope.name == name && scope.endIndex == 0) {
                scope.endIndex = idx;
                break;
            }
        }
    }

    void GpuProfiler::RetrieveResults() {
        if (_currentQuery == 0) return;

        std::vector<uint64_t> queryResults(_currentQuery);
        VkResult res = vkGetQueryPoolResults(
            _device->getDevice(), 
            _queryPool, 
            0, _currentQuery, 
            _currentQuery * sizeof(uint64_t), 
            queryResults.data(), 
            sizeof(uint64_t), 
            VK_QUERY_RESULT_64_BIT
        );

        if (res == VK_SUCCESS) {
            for (const auto& scope : _activeScopes) {
                if (scope.endIndex == 0) continue; // Unfinished scope

                uint64_t start = queryResults[scope.startIndex];
                uint64_t end = queryResults[scope.endIndex];
                
                float durationNs = (end - start) * _timestampPeriod;
                float durationMs = durationNs / 1000000.0f;

                _results[scope.name] = durationMs;
                // Optional: Print or store
                // LOG_INFO("GPU Profile [" + scope.name + "]: " + std::to_string(durationMs) + "ms");
            }
        }
    }
}
