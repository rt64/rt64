#include <iostream>
#include <fstream>
#include <string>

int extract_id(const std::string& line) {
    size_t startPos = line.find("[[id(");
    std::string id = "-1";
    if (startPos != std::string::npos) {
        startPos += 5; // Move past "[[id("
        size_t endPos = line.find(')', startPos);
        if (endPos != std::string::npos) {
            id = line.substr(startPos, endPos - startPos);
        }
    }
    return std::stoi(id);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.tmp.metal> <output.metal>\n";
    }

    const char* input_path = argv[1];
    const char* output_path = argv[2];

    std::ifstream input_file(input_path);
    std::ofstream output_file{output_path};

    std::string default_resource_prefix = "\tconstant float* _PadResource";

    if (input_file.is_open()) {
        std::string line;
        bool inside_set = false;
        uint32_t current_id = 0;

        while (std::getline(input_file, line)) {
            if (line.find("struct spvDescriptorSetBuffer") != std::string::npos) {
                inside_set = true;
                current_id = 0;
                output_file << line << std::endl;
                std::getline(input_file, line); // Get the starting brace '{'
            } else if (inside_set) {
                if (line == "};") {
                    inside_set = false;
                    output_file << line << std::endl;
                    continue;
                }
                int line_id = extract_id(line);
                if (line_id == -1) {
                    throw std::runtime_error("Failed to extract id from line: " + line);
                }
                while (current_id != line_id) {
                    output_file << default_resource_prefix << current_id << " [[id(" << current_id << ")]];\n";
                    current_id++;
                }
                // Current line id consumes the current id
                current_id++;
            }
            output_file << line << std::endl;
        }
    }
}