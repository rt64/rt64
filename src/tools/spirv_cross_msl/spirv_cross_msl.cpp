#include "spirv_msl.hpp"
#include "spirv_parser.hpp"
#include <fstream>
#include <iostream>
#include <vector>

static std::vector<uint32_t> read_spirv_file_stdin() {
#ifdef _WIN32
	setmode(fileno(stdin), O_BINARY);
#endif

	std::vector<uint32_t> buffer;
	uint32_t tmp[256];
	size_t ret;

	while ((ret = fread(tmp, sizeof(uint32_t), 256, stdin)))
		buffer.insert(buffer.end(), tmp, tmp + ret);

	return buffer;
}

static std::vector<uint32_t> read_spirv_file(const char *path) {
	if (path[0] == '-' && path[1] == '\0')
		return read_spirv_file_stdin();

	FILE *file = fopen(path, "rb");
	if (!file)
	{
		fprintf(stderr, "Failed to open SPIR-V file: %s\n", path);
		return {};
	}

	fseek(file, 0, SEEK_END);
	long len = ftell(file) / sizeof(uint32_t);
	rewind(file);

	std::vector<uint32_t> spirv(len);
	if (fread(spirv.data(), sizeof(uint32_t), len, file) != size_t(len))
		spirv.clear();

	fclose(file);
	return spirv;
}

static bool write_string_to_file(const char *path, const char *string) {
	FILE *file = fopen(path, "w");
	if (!file)
	{
		fprintf(stderr, "Failed to write file: %s\n", path);
		return false;
	}

	fprintf(file, "%s", string);
	fclose(file);
	return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.spv> <output.metal>\n";
        return 1;
    }

    try {
        // Load SPIR-V
        auto spirv_file = read_spirv_file(argv[1]);
        spirv_cross::Parser spirv_parser(std::move(spirv_file));
	    spirv_parser.parse();
        
        // Initialize MSL compiler
        spirv_cross::CompilerMSL msl(std::move(spirv_parser.get_parsed_ir()));
        
        // Configure MSL options
        spirv_cross::CompilerMSL::Options msl_options;
        msl_options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 1);
        msl_options.argument_buffers = true;
        msl_options.texture_buffer_native = true;
        msl_options.enable_decoration_binding = true;
        msl.set_msl_options(msl_options);

        spirv_cross::MSLResourceBinding msl_binding_vert;
        msl_binding_vert.stage = spv::ExecutionModelVertex;
        msl_binding_vert.desc_set = spirv_cross::kPushConstDescSet;
        msl_binding_vert.binding = spirv_cross::kPushConstBinding;
        msl_binding_vert.msl_buffer = 8;
        msl.add_msl_resource_binding(msl_binding_vert);

        spirv_cross::MSLResourceBinding msl_binding_frag;
        msl_binding_frag.stage = spv::ExecutionModelFragment;
        msl_binding_frag.desc_set = spirv_cross::kPushConstDescSet;
        msl_binding_frag.binding = spirv_cross::kPushConstBinding;
        msl_binding_frag.msl_buffer = 8;
        msl.add_msl_resource_binding(msl_binding_frag);

        spirv_cross::MSLResourceBinding msl_binding_compute;
        msl_binding_compute.stage = spv::ExecutionModelGLCompute;
        msl_binding_compute.desc_set = spirv_cross::kPushConstDescSet;
        msl_binding_compute.binding = spirv_cross::kPushConstBinding;
        msl_binding_compute.msl_buffer = 8;
        msl.add_msl_resource_binding(msl_binding_compute);

        // Configure common options
        spirv_cross::CompilerGLSL::Options common_options;
        common_options.vertex.flip_vert_y = true;
        msl.set_common_options(common_options);

        // Generate MSL source
        std::string source = msl.compile();
        write_string_to_file(argv[2], source.c_str());
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
