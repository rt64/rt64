#include <filesystem>
#include <fstream>
#include <cstdio>
#include <vector>

std::vector<char> read_file(const char* path) {
    std::ifstream input_file{path, std::ios::binary};
    std::vector<char> ret{};

    if (!input_file.good()) {
        return ret;
    }

    // Get the length of the file
    input_file.seekg(0, std::ios::end);
    ret.resize(input_file.tellg());
    
    // Read the file contents into the vector
    input_file.seekg(0, std::ios::beg);
    input_file.read(ret.data(), ret.size());

    return ret;
}

void create_parent_if_needed(const char* path) {
    std::filesystem::path parent_path = std::filesystem::path{path}.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path);
    }
}

int main(int argc, const char** argv) {
    if (argc != 5) {
        printf("Usage: %s [input file] [array name] [output C file] [output C header]\n", argv[0]);
        return EXIT_SUCCESS;
    }

    const char* input_path = argv[1];
    const char* array_name = argv[2];
    const char* output_c_path = argv[3];
    const char* output_h_path = argv[4];

    // Read the input file's contents
    std::vector<char> contents = read_file(input_path);

    if (contents.empty()) {
        fprintf(stderr, "Failed to open file %s! (Or it's empty)\n", input_path);
        return EXIT_FAILURE;
    }

    // Create the output directories if they don't exist
    create_parent_if_needed(output_c_path);
    create_parent_if_needed(output_h_path);

    // Write the C file with the array
    {
        std::ofstream output_c_file{output_c_path};
        output_c_file << "extern const char " << array_name << "[" << contents.size() << "];\n";
        output_c_file << "const char " << array_name << "[" << contents.size() << "] = {";

        for (char x : contents) {
            output_c_file << (int)x << ", ";
        }

        output_c_file << "};\n";
    }

    // Write the header file with the extern array
    {
        std::ofstream output_h_file{output_h_path};
        output_h_file <<
            "#ifdef __cplusplus\n"
            "  extern \"C\" {\n"
            "#endif\n"
            "extern const char " << array_name << "[" << contents.size() << "];\n"
            "#ifdef __cplusplus\n"
            "  }\n"
            "#endif\n";
    }
}
