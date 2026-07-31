#include "NVSCompiler.hpp"
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace Cogent::Scripting::NVS {

    std::vector<VisualNode> NVSCompiler::RemoveDeadNodes(const std::vector<VisualNode>& inputGraph) {
        std::vector<VisualNode> optimized;
        std::unordered_set<uint32_t> activeNodeIds;

        // Proper recursive backward traversal
        std::vector<uint32_t> stack;
        for (const auto& node : inputGraph) {
            if (node.hasExecutionPin) {
                stack.push_back(node.id);
            }
        }

        while (!stack.empty()) {
            uint32_t currentId = stack.back();
            stack.pop_back();

            if (activeNodeIds.find(currentId) == activeNodeIds.end()) {
                activeNodeIds.insert(currentId);
                // Find node and push its inputs
                for (const auto& node : inputGraph) {
                    if (node.id == currentId) {
                        for (uint32_t inId : node.inputNodeIds) {
                            stack.push_back(inId);
                        }
                        break;
                    }
                }
            }
        }

        int removedCount = 0;
        for (const auto& node : inputGraph) {
            if (activeNodeIds.find(node.id) != activeNodeIds.end()) {
                optimized.push_back(node);
            } else {
                removedCount++;
            }
        }

        if (removedCount > 0) {
            std::cout << "[NVS Compiler] Dead Node Removal (DNR): Removed " << removedCount << " unused nodes.\n";
        }
        return optimized;
    }

    std::vector<Instruction> NVSCompiler::Compile(const std::vector<VisualNode>& graphNodes) {
        std::vector<Instruction> bytecode;
        
        std::cout << "[NVS Compiler] Compiling " << graphNodes.size() << " Visual Nodes...\n";
        
        auto optimizedGraph = RemoveDeadNodes(graphNodes);

        // Very basic mock compiler: assumes topological sort is already done
        for (const auto& node : optimizedGraph) {
            switch (node.type) {
                case NodeType::LITERAL_FLOAT:
                    bytecode.push_back({Opcode::PUSH_FLOAT, node.value});
                    break;
                case NodeType::MATH_ADD:
                    bytecode.push_back({Opcode::ADD, 0.0f});
                    break;
                case NodeType::MATH_SUB:
                    bytecode.push_back({Opcode::SUB, 0.0f});
                    break;
                case NodeType::MATH_MUL:
                    bytecode.push_back({Opcode::MUL, 0.0f});
                    break;
                case NodeType::DEBUG_LOG:
                    bytecode.push_back({Opcode::PRINT, 0.0f});
                    break;
            }
        }

        bytecode.push_back({Opcode::HALT, 0.0f});
        
        std::cout << "[NVS Compiler] Compiled successfully into " << bytecode.size() << " instructions.\n";
        return bytecode;
    }
}
