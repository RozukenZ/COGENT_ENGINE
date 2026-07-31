#pragma once
#include <vector>
#include <cstdint>

namespace Cogent::Scripting::NVS {

    // NVS Virtual Machine Instruction Opcodes
    enum class Opcode : uint8_t {
        NOP = 0,
        PUSH_FLOAT, // Push literal float to stack
        ADD,        // Pop A, Pop B, Push A+B
        SUB,        // Pop A, Pop B, Push A-B
        MUL,        // Pop A, Pop B, Push A*B
        PRINT,      // Pop A, Print A
        HALT        // End of execution
    };

    // A single instruction in Bytecode
    struct Instruction {
        Opcode op;
        float operand; // For simplicity, our VM uses floats for data
    };

    // Virtual Machine Execution Engine
    class NVSVM {
    public:
        NVSVM();
        ~NVSVM();

        // Execute a compiled Bytecode stream
        void Execute(const std::vector<Instruction>& bytecode);

        // Get the top of the stack (useful for testing)
        float PeekStackTop() const;

    private:
        std::vector<float> _stack;
    };
}
