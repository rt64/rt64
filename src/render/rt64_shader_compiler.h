//
// RT64
//

#pragma once

#if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)

#include <Windows.h>
#include <dxcapi.h>

#include <string>

#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct ShaderCompiler {
        IDxcCompiler *dxcCompiler = nullptr;
        IDxcUtils *dxcUtils = nullptr;

        ShaderCompiler();
        ~ShaderCompiler();
        void compile(const std::string &shaderCode, const std::wstring &entryName, const std::wstring &profile,
            RenderShaderFormat shaderFormat, IDxcBlob **shaderBlob) const;
    };
};

#else

// Shader compiler is not required on other platforms at runtime.

namespace RT64 {
    typedef void* ShaderCompiler;
};

#endif

