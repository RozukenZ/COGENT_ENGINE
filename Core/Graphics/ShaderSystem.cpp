#include "ShaderSystem.hpp"
#include "../Logger.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <stdexcept>
#include "../EmbeddedShaders.hpp"

namespace Cogent {
namespace Graphics {

ShaderSystem::ShaderSystem(GraphicsDevice& device) : m_device(device) {
}

ShaderSystem::~ShaderSystem() {
    clearCache();
}

void ShaderSystem::clearCache() {
    for (auto& pair : m_moduleCache) {
        vkDestroyShaderModule(m_device.getDevice(), pair.second, nullptr);
    }
    m_moduleCache.clear();
}

std::vector<char> ShaderSystem::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

std::vector<char> ShaderSystem::compileShaderToSpirv(const std::string& shaderPath, const std::vector<std::string>& defines) {
    std::string outPath = shaderPath + ".tmp.spv";
    
    std::string cmd = "glslc \"" + shaderPath + "\"";
    for (const auto& def : defines) {
        cmd += " -D" + def;
    }
    cmd += " -o \"" + outPath + "\"";
    
    // LOG_INFO("Compiling shader: " + cmd);
    int result = std::system(cmd.c_str());
    if (result != 0) {
        LOG_ERROR("Failed to compile shader: " + shaderPath);
        throw std::runtime_error("glslc failed for " + shaderPath);
    }
    
    std::vector<char> spirv = readFile(outPath);
    std::remove(outPath.c_str()); // cleanup
    return spirv;
}

VkShaderModule ShaderSystem::getShaderModule(const std::string& shaderPath, const std::vector<std::string>& defines) {
    std::string cacheKey = shaderPath;
    for (const auto& def : defines) {
        cacheKey += "|" + def;
    }
    
    if (m_moduleCache.find(cacheKey) != m_moduleCache.end()) {
        return m_moduleCache[cacheKey];
    }
    
    // Try to load embedded first if no defines, else compile via glslc
    std::vector<char> code;
    if (defines.empty()) {
        // Try embedded shaders first
        std::string spvPath = shaderPath + ".spv";
        code = EmbeddedShaders::GetShader(spvPath);
        
        if (code.empty()) {
            LOG_WARN("Embedded shader not found: " + spvPath + ". Falling back to dynamic compilation.");
            code = compileShaderToSpirv(shaderPath, defines);
        }
    } else {
        code = compileShaderToSpirv(shaderPath, defines);
    }
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module for " + shaderPath);
    }
    
    m_moduleCache[cacheKey] = shaderModule;
    return shaderModule;
}

} // namespace Graphics
} // namespace Cogent
