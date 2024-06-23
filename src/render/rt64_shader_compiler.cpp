
//;
// RT64
//

#if defined(_WIN32) && !defined(RT64_BUILD_PLUGIN)

#include "rt64_shader_compiler.h"

#include "common/rt64_common.h"

namespace RT64 {
    // ShaderCompiler

    ShaderCompiler::ShaderCompiler() {
        HRESULT res = DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void **)(&dxcCompiler));
        if (FAILED(res)) {
            fprintf(stderr, "DxcCreateInstance(DxcCompiler) failed with error code 0x%X.\n", res);
            return;
        }

        res = DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils), (void **)(&dxcUtils));
        if (FAILED(res)) {
            fprintf(stderr, "DxcCreateInstance(DxcUtils) failed with error code 0x%X.\n", res);
            return;
        }
    }

    ShaderCompiler::~ShaderCompiler() {
        if (dxcCompiler != nullptr) {
            dxcCompiler->Release();
        }

        if (dxcUtils != nullptr) {
            dxcUtils->Release();
        }
    }

    void ShaderCompiler::compile(const std::string &shaderCode, const std::wstring &entryName, const std::wstring &profile,
        RenderShaderFormat shaderFormat, IDxcBlob **shaderBlob) const
    {
        IDxcBlobEncoding *textBlob = nullptr;
        dxcUtils->CreateBlobFromPinned((LPBYTE)shaderCode.c_str(), (uint32_t)shaderCode.size(), DXC_CP_ACP, &textBlob);

        std::vector<LPCWSTR> arguments;
        arguments.push_back(L"-Qstrip_debug");

        switch (shaderFormat) {
        case RenderShaderFormat::DXIL:
            arguments.push_back(L"-Qstrip_reflect");
            break;
        case RenderShaderFormat::SPIRV:
            arguments.push_back(L"-spirv");
            arguments.push_back(L"-fvk-use-dx-layout");

            if (profile.find(L"vs") != std::wstring::npos) {
                arguments.push_back(L"-fvk-invert-y");
            }
            else if (profile.find(L"lib") != std::wstring::npos) {
                arguments.push_back(L"-fspv-target-env=vulkan1.1spirv1.4");
                arguments.push_back(L"-fspv-extension=SPV_KHR_ray_tracing");
            }

            break;
        default:
            assert(false && "Unknown shader format.");
            return;
        }

        IDxcOperationResult *result = nullptr;
        dxcCompiler->Compile(textBlob, L"", entryName.c_str(), profile.c_str(), arguments.data(), (UINT32)(arguments.size()), nullptr, 0, nullptr, &result);

        HRESULT resultCode;
        result->GetStatus(&resultCode);
        if (FAILED(resultCode)) {
            IDxcBlobEncoding *error;
            HRESULT hr = result->GetErrorBuffer(&error);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to get shader compiler error");
            }

            // Convert error blob to a string.
            std::vector<char> infoLog(error->GetBufferSize() + 1);
            memcpy(infoLog.data(), error->GetBufferPointer(), error->GetBufferSize());
            infoLog[error->GetBufferSize()] = 0;

            RT64_LOG_PRINTF("Shader: %s\n", shaderCode.data());
            RT64_LOG_PRINTF("Shader compilation error: %s\n", infoLog.data());
            throw std::runtime_error("Shader compilation error: " + std::string(infoLog.data()));
        }

        result->GetResult(shaderBlob);
    }
};

#endif