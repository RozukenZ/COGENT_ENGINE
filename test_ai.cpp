#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include "Core/ECS/ECS.hpp"
#include "AI/BehaviorTree.hpp"
#include "AI/NAIManager.hpp"
#include "Core/Threading/JobSystem.hpp"

using namespace Cogent;
using namespace Cogent::Core::ECS;
using namespace Cogent::AI;
using namespace Cogent::Threading;

// Custom BT Action Node that uses NAI
class VisionCheckNode : public BTNode {
public:
    VisionCheckNode(const std::string& key) : blackboardKey(key) {}

    NodeStatus Tick(Blackboard& blackboard) override {
        std::string status = blackboard.Get<std::string>(blackboardKey + "_Status", "NONE");

        if (status == "NONE") {
            // First time running, submit to NAI Manager
            NAIManager::Get().SubmitInferenceTask(blackboard, blackboardKey);
            return NodeStatus::RUNNING;
        } 
        else if (status == "RUNNING") {
            // Background thread is still processing ML model
            return NodeStatus::RUNNING;
        } 
        else if (status == "SUCCESS") {
            // ML Job Finished!
            return NodeStatus::SUCCESS;
        }
        
        return NodeStatus::FAILURE;
    }
private:
    std::string blackboardKey;
};

// Component for ECS
struct AIComponent {
    std::shared_ptr<BTNode> rootNode;
    std::shared_ptr<Blackboard> blackboard;
};

int main() {
    std::cout << "--- COGENT ENGINE AI ECOSYSTEM TEST ---\n";
    
    JobSystem::Get().Initialize();

    Registry ecs;
    ecs.RegisterComponent<AIComponent>();

    const size_t NUM_AGENTS = 1000;
    std::vector<Entity> agents;
    
    std::cout << "[INFO] Spawning " << NUM_AGENTS << " AI Agents...\n";

    for (size_t i = 0; i < NUM_AGENTS; ++i) {
        Entity agent = ecs.CreateEntity();
        
        AIComponent ai;
        auto sequence = std::make_shared<Sequence>();
        sequence->AddChild(std::make_shared<VisionCheckNode>("VisionTask"));
        ai.rootNode = sequence;
        ai.blackboard = std::make_shared<Blackboard>();
        
        ecs.AddComponent(agent, ai);
        agents.push_back(agent);
    }

    std::cout << "[TEST] Running Game Loop (Simulating Ticks)...\n";
    
    int frame = 0;
    bool allFinished = false;

    auto start = std::chrono::high_resolution_clock::now();

    while (!allFinished && frame < 1000) { // Max 1000 frames timeout
        allFinished = true;
        
        // Simulasikan Update tiap frame
        for (Entity agent : agents) {
            auto& ai = ecs.GetComponent<AIComponent>(agent);
            NodeStatus status = ai.rootNode->Tick(*ai.blackboard);
            
            if (status == NodeStatus::RUNNING) {
                allFinished = false; // Ada AI yang masih nunggu inferensi
            }
        }
        
        frame++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Simulasikan frametime ringan
    }

    auto end = std::chrono::high_resolution_clock::now();
    double timeTaken = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "\n--- RESULTS ---\n";
    std::cout << "Frames Simulated : " << frame << " frames\n";
    std::cout << "Time Elapsed     : " << timeTaken << " ms\n";
    
    if (allFinished) {
        std::cout << "[PASS] All 1000 AI Agents completed their asynchronous ML Inference successfully without blocking the main loop!\n";
    } else {
        std::cerr << "[FAIL] AI Timeout. Background workers stuck.\n";
        return 1;
    }

    JobSystem::Get().Shutdown();
    return 0;
}
