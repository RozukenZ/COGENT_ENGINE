#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <shared_mutex>
#include <any>

namespace Cogent::AI {

    // Thread-safe Blackboard for ML jobs to write to without blocking main thread.
    class Blackboard {
    public:
        template <typename T>
        void Set(const std::string& key, T value) {
            std::unique_lock lock(mutex);
            data[key] = value;
        }

        template <typename T>
        T Get(const std::string& key, T defaultValue = T()) {
            std::shared_lock lock(mutex);
            auto it = data.find(key);
            if (it != data.end()) {
                try {
                    return std::any_cast<T>(it->second);
                } catch (const std::bad_any_cast&) {
                    return defaultValue;
                }
            }
            return defaultValue;
        }

    private:
        std::unordered_map<std::string, std::any> data;
        std::shared_mutex mutex;
    };

    enum class NodeStatus {
        SUCCESS,
        FAILURE,
        RUNNING
    };

    class BTNode {
    public:
        virtual ~BTNode() = default;
        virtual NodeStatus Tick(Blackboard& blackboard) = 0;
    };

    class Sequence : public BTNode {
    public:
        void AddChild(std::shared_ptr<BTNode> child) { children.push_back(child); }

        NodeStatus Tick(Blackboard& blackboard) override {
            for (auto& child : children) {
                NodeStatus status = child->Tick(blackboard);
                if (status != NodeStatus::SUCCESS) {
                    return status; // Return FAILURE or RUNNING
                }
            }
            return NodeStatus::SUCCESS;
        }
    private:
        std::vector<std::shared_ptr<BTNode>> children;
    };

    class Selector : public BTNode {
    public:
        void AddChild(std::shared_ptr<BTNode> child) { children.push_back(child); }

        NodeStatus Tick(Blackboard& blackboard) override {
            for (auto& child : children) {
                NodeStatus status = child->Tick(blackboard);
                if (status != NodeStatus::FAILURE) {
                    return status; // Return SUCCESS or RUNNING
                }
            }
            return NodeStatus::FAILURE;
        }
    private:
        std::vector<std::shared_ptr<BTNode>> children;
    };

}
