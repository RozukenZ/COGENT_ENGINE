#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "GraphicsDevice.hpp"

namespace Cogent {
namespace Graphics {

class ShaderSystem {
public:
    ShaderSystem(GraphicsDevice& device);
    ~ShaderSystem();

    // Retrieves a shader module. Compiles dynamically if not cached or if variants (defines) are used.
    // Defines format: "HAS_SHADOWS=1" or "HAS_NORMAL_MAP"
    VkShaderModule getShaderModule(const std::string& shaderPath, const std::vector<std::string>& defines = {});

    // Clears the cache, forcing recompilation next time (useful for hot-reloading)
    void clearCache();

private:
    std::vector<char> compileShaderToSpirv(const std::string& shaderPath, const std::vector<std::string>& defines);
    std::vector<char> readFile(const std::string& filename);

    GraphicsDevice& m_device;
    std::unordered_map<std::string, VkShaderModule> m_moduleCache;
};

} // namespace Graphics
} // namespace Cogent
