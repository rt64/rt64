//
// RT64
//

#pragma once

#if defined(_WIN32)

#include <Windows.h>
#include <dxcapi.h>

#include <string>

#include "common/rt64_plume.h"

namespace RT64 {
    struct ShaderCompiler {
        IDxcCompiler *dxcCompiler = nullptr;
        IDxcUtils *dxcUtils = nullptr;

        ShaderCompiler();
        ~ShaderCompiler();

        void compile(const std::string &shaderCode, const std::wstring &entryName, const std::wstring &profile,
            RenderShaderFormat shaderFormat, IDxcBlob **shaderBlob) const;

        void link(const std::wstring &entryName, const std::wstring &profile, IDxcBlob **libraryBlobs,
            const wchar_t **libraryBlobNames, uint32_t libraryBlobCount, IDxcBlob **shaderBlob) const;
    };
};

#else

// Shader compiler is not required on other platforms at runtime.

namespace RT64 {
    typedef void* ShaderCompiler;
};

#endif

