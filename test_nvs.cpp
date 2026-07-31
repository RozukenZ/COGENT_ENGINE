#include <iostream>
#include <vector>
#include <chrono>
#include "Scripting/NVS/NVSCompiler.hpp"
#include "Scripting/NVS/NVSVM.hpp"

using namespace Cogent::Scripting::NVS;

int main() {
    std::cout << "--- COGENT ENGINE NVS STUDIO TEST ---\n";

    // 1. Create a Mock Graph from the Visual UI
    // Logic: 
    // Node 1 (Value: 10.5)
    // Node 2 (Value: 20.0)
    // Node 3 (Add Node 1 + Node 2)
    // Node 4 (Print Node 3) -> hasExecutionPin = true
    // Node 5 (Value: 99.9) -> DEAD NODE (disconnected)
    
    std::vector<VisualNode> graph;
    
    VisualNode n1; n1.id = 1; n1.type = NodeType::LITERAL_FLOAT; n1.value = 10.5f;
    graph.push_back(n1);

    VisualNode n2; n2.id = 2; n2.type = NodeType::LITERAL_FLOAT; n2.value = 20.0f;
    graph.push_back(n2);

    VisualNode n3; n3.id = 3; n3.type = NodeType::MATH_ADD; n3.inputNodeIds = {1, 2};
    graph.push_back(n3);
    
    // Add the execution node that hooks everything
    VisualNode n4; n4.id = 4; n4.type = NodeType::DEBUG_LOG; n4.inputNodeIds = {3}; n4.hasExecutionPin = true;
    graph.push_back(n4);

    // Dead node
    VisualNode n5; n5.id = 5; n5.type = NodeType::LITERAL_FLOAT; n5.value = 99.9f; 
    // Note: n5 has no execution pin and isn't linked to n4's inputs
    graph.push_back(n5);

    // 2. Compile Graph
    NVSCompiler compiler;
    auto startCompile = std::chrono::high_resolution_clock::now();
    std::vector<Instruction> bytecode = compiler.Compile(graph);
    auto endCompile = std::chrono::high_resolution_clock::now();
    
    // 3. Execute Bytecode in VM
    NVSVM vm;
    auto startEx = std::chrono::high_resolution_clock::now();
    
    // We execute it 100,000 times to test VM overhead
    for(int i = 0; i < 100000; i++) {
        vm.Execute(bytecode);
    }
    
    auto endEx = std::chrono::high_resolution_clock::now();

    double compileTime = std::chrono::duration<double, std::micro>(endCompile - startCompile).count();
    double execTimeTotal = std::chrono::duration<double, std::milli>(endEx - startEx).count();

    std::cout << "\n--- NVS RESULTS ---\n";
    std::cout << "Compile Time      : " << compileTime << " us (microseconds)\n";
    std::cout << "VM Exec (100k)    : " << execTimeTotal << " ms\n";
    std::cout << "Instr per Bytecode: " << bytecode.size() << "\n";

    // Since Dead Node Removal works, n5 is removed, leaving 4 instructions + 1 HALT = 5 instructions
    if (bytecode.size() == 5) {
        std::cout << "[PASS] Dead Node Removal successfully optimized the graph!\n";
    } else {
        std::cerr << "[FAIL] Optimizer failed. Dead node was not removed.\n";
        return 1;
    }

    if (execTimeTotal < 50.0) {
        std::cout << "[PASS] VM Execution overhead is incredibly low!\n";
    } else {
        std::cerr << "[FAIL] VM is too slow for real-time gameplay logic.\n";
        return 1;
    }

    return 0;
}
