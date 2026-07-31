#pragma once
#include <vector>
#include <memory>
#include <string>
#include "NVSVM.hpp"

namespace Cogent::Scripting::NVS {

    enum class NodeType {
        LITERAL_FLOAT,
        MATH_ADD,
        MATH_SUB,
        MATH_MUL,
        DEBUG_LOG
    };

    // A mock representation of a Visual Node in the Graph Editor
    struct VisualNode {
        uint32_t id;
        NodeType type;
        float value = 0.0f; // For LITERAL_FLOAT
        
        // Data flow pins (In a real system, nodes have multiple Input/Output pins)
        std::vector<uint32_t> inputNodeIds;
        
        // Is this node connected to an execution flow? (Used for Dead Node Removal)
        bool hasExecutionPin = false; 
    };

    class NVSCompiler {
    public:
        // Takes a list of raw VisualNodes from the UI Editor and compiles to Bytecode
        std::vector<Instruction> Compile(const std::vector<VisualNode>& graphNodes);

    private:
        // Optimizer: Removes nodes that do not contribute to final execution
        std::vector<VisualNode> RemoveDeadNodes(const std::vector<VisualNode>& inputGraph);
    };
}
