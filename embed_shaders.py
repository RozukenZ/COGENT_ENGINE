import os
import sys

def main():
    shaders_dir = "Shaders"
    output_hpp = "Core/EmbeddedShaders.hpp"
    output_cpp = "Core/EmbeddedShaders.cpp"

    if not os.path.exists(shaders_dir):
        print(f"Error: {shaders_dir} not found")
        sys.exit(1)

    spv_files = [f for f in os.listdir(shaders_dir) if f.endswith(".spv")]

    with open(output_hpp, "w") as f_hpp, open(output_cpp, "w") as f_cpp:
        f_hpp.write("#pragma once\n")
        f_hpp.write("#include <vector>\n")
        f_hpp.write("#include <string>\n")
        f_hpp.write("#include <unordered_map>\n\n")
        f_hpp.write("namespace EmbeddedShaders {\n")
        f_hpp.write("    std::vector<char> GetShader(const std::string& name);\n")
        f_hpp.write("}\n")

        f_cpp.write("#include \"EmbeddedShaders.hpp\"\n\n")
        f_cpp.write("namespace EmbeddedShaders {\n")
        
        # Write arrays
        array_names = []
        for spv in spv_files:
            var_name = spv.replace(".", "_")
            array_names.append((spv, var_name))
            
            with open(os.path.join(shaders_dir, spv), "rb") as f:
                data = f.read()
            
            f_cpp.write(f"    const unsigned char {var_name}[] = {{\n        ")
            f_cpp.write(", ".join(f"0x{b:02x}" for b in data))
            f_cpp.write("\n    };\n")
            f_cpp.write(f"    const unsigned int {var_name}_size = {len(data)};\n\n")
            
        # Write map
        f_cpp.write("    std::vector<char> GetShader(const std::string& name) {\n")
        for spv, var_name in array_names:
            f_cpp.write(f"        if (name == \"Shaders/{spv}\" || name == \"{spv}\") {{\n")
            f_cpp.write(f"            return std::vector<char>({var_name}, {var_name} + {var_name}_size);\n")
            f_cpp.write(f"        }}\n")
        
        f_cpp.write("        return std::vector<char>();\n")
        f_cpp.write("    }\n")
        f_cpp.write("}\n")

if __name__ == "__main__":
    main()
