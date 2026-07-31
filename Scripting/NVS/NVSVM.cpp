#include "NVSVM.hpp"
#include <iostream>

namespace Cogent::Scripting::NVS {

    NVSVM::NVSVM() {
        _stack.reserve(256); // Allocate small initial stack
    }

    NVSVM::~NVSVM() {}

    void NVSVM::Execute(const std::vector<Instruction>& bytecode) {
        _stack.clear();

        for (size_t ip = 0; ip < bytecode.size(); ++ip) {
            const auto& inst = bytecode[ip];
            
            switch (inst.op) {
                case Opcode::NOP:
                    break;
                case Opcode::PUSH_FLOAT:
                    _stack.push_back(inst.operand);
                    break;
                case Opcode::ADD: {
                    if (_stack.size() < 2) continue;
                    float b = _stack.back(); _stack.pop_back();
                    float a = _stack.back(); _stack.pop_back();
                    _stack.push_back(a + b);
                    break;
                }
                case Opcode::SUB: {
                    if (_stack.size() < 2) continue;
                    float b = _stack.back(); _stack.pop_back();
                    float a = _stack.back(); _stack.pop_back();
                    _stack.push_back(a - b);
                    break;
                }
                case Opcode::MUL: {
                    if (_stack.size() < 2) continue;
                    float b = _stack.back(); _stack.pop_back();
                    float a = _stack.back(); _stack.pop_back();
                    _stack.push_back(a * b);
                    break;
                }
                case Opcode::PRINT: {
                    if (_stack.empty()) continue;
                    float val = _stack.back(); _stack.pop_back();
                    // std::cout << "[NVS VM Log] Output: " << val << "\n"; // Disabled for performance
                    break;
                }
                case Opcode::HALT:
                    return;
            }
        }
    }

    float NVSVM::PeekStackTop() const {
        if (_stack.empty()) return 0.0f;
        return _stack.back();
    }
}
