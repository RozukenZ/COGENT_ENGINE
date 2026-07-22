#pragma once
#include <vector>
#include <string>
#include <unordered_map>

namespace EmbeddedShaders {
    std::vector<char> GetShader(const std::string& name);
}
