#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>

// Mewakili satu tahap di dalam pipeline (misal: GBufferPass, TAA_Pass)
struct RenderPassNode {
    std::string name;
    std::function<void(VkCommandBuffer)> executeCallback;
    // Di sini nantinya bisa ditambah 'reads' dan 'writes' untuk automatic barriers
};

class RenderGraph {
public:
    void addPass(const std::string& name, std::function<void(VkCommandBuffer)> logic) {
        RenderPassNode node;
        node.name = name;
        node.executeCallback = logic;
        passes.push_back(node);
    }

    // Menjalankan semua pass secara berurutan
    void execute(VkCommandBuffer cmd) {
        for (auto& pass : passes) {
            // Placeholder untuk Image Barrier (Sinkronisasi otomatis)
            // Logika: Jika pass ini butuh input dari pass sebelumnya, pasang Barrier di sini.
            
            pass.executeCallback(cmd);
        }
    }

    void clear() {
        passes.clear();
    }

private:
    std::vector<RenderPassNode> passes;
};