//
// RT64
//

/*
 * Integration with librashader for postprocessing
 *
 *
librashader feature dev:
TODO/ISSUES:
- Metal support is missing
- Handle graphic backends dynamically
- Preprocessor cleanup for non-librashader builds
*/

#include "rt64_librashader.h"

#include <vector>

#ifdef _WIN32
#include "plume_d3d12.h"
#define LIBRA_RUNTIME_D3D12 1
#endif

#include "plume_vulkan.h"
//#define LIBRA_RUNTIME_VULKAN 1

// todo: metal

#include "librashader.h"
#include "librashader_ld.h"

namespace RT64 {
    // Librashader
    libra_instance_t libra;
    libra_shader_preset_t preset = nullptr;

    libra_preset_ctx_t libraContext;

#if LIBRA_RUNTIME_D3D12
    libra_d3d12_filter_chain_t dx_filterChain = nullptr;
#endif

#if LIBRA_RUNTIME_VULKAN
    libra_vk_filter_chain_t vk_filterChain = nullptr;
#endif

    std::vector<LibraRuntimeParam> currentRuntimeParams;
    std::string currentShaderPath;

    Librashader::Librashader() {
        libra = librashader_load_instance();
    }

    Librashader::~Librashader() { 
        // todo: cleanup of libra instance?
    }

    std::string Librashader::getCurrentShader() {
        return currentShaderPath;
    }

    std::vector<LibraRuntimeParam>& Librashader::getRuntimeParams() {
        return currentRuntimeParams;
    }

    void Librashader::updateRuntimeParam(LibraRuntimeParam parameter) {
#if LIBRA_RUNTIME_D3D12
        if (dx_filterChain) {
            libra.d3d12_filter_chain_set_param(&dx_filterChain, parameter.name.c_str(), parameter.current_value);
        }
#endif

#if LIBRA_RUNTIME_VULKAN
        if (vk_filterChain) {
            libra.vk_filter_chain_set_param(&vk_filterChain, parameter.name.c_str(), parameter.current_value);
        }
#endif
    }

    bool Librashader::updateShader(RenderDevice* device, const std::string desiredShader) {
        if (desiredShader != getCurrentShader()) {
            if (desiredShader.empty())
                reset();
            else
                setup(device, desiredShader);

            return true;
        }

        return false;
    }

    void Librashader::postprocess(const Librashader::LibraFrameParams &lp) {
        // librashader requires commandlist to be wrapped for current phase
        lp.commandList->end();

        const RenderCommandList *constCmd = lp.worker->commandList.get();
        lp.worker->commandQueue->executeCommandLists(&constCmd, 1, nullptr, 0, nullptr, 0, lp.worker->commandFence.get());
        lp.worker->wait();

        // new list for menu to avoid libra corruption
        lp.commandList->begin();

        lp.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(lp.inputTexture, RenderTextureLayout::SHADER_READ));
        lp.commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(lp.outputTexture, RenderTextureLayout::COLOR_WRITE));
        lp.commandList->setFramebuffer(lp.outputFramebuffer);

#ifdef LIBRA_RUNTIME_VULKAN
        // Vulkan seems to explode without a "fresh" commandlist with nothing done to it
        lp.commandList->end();
#endif

        // librashader hookup
        libra_viewport_t viewport = { lp.viewport.x, lp.viewport.y, lp.viewport.width, lp.viewport.height };

        // todo: Needs to work more like inspector, check active config
#if LIBRA_RUNTIME_D3D12
        auto* d3d12CmdList = static_cast<plume::D3D12CommandList*>(lp.commandList)->d3d;
        auto* d3d12Input = static_cast<plume::D3D12Texture*>(lp.inputTexture)->d3d;
        auto* d3d12Output = static_cast<plume::D3D12Texture*>(lp.outputTexture)->d3d;

        libra_image_d3d12_handle_t input_handle = { d3d12Input };
        libra_image_d3d12_handle_t output_handle = { d3d12Output };
        libra_image_d3d12_t input = {};
        libra_image_d3d12_t output = {};
        input.image_type = LIBRA_D3D12_IMAGE_TYPE_RESOURCE;
        output.image_type = LIBRA_D3D12_IMAGE_TYPE_RESOURCE;
        input.handle = input_handle;
        output.handle = output_handle;

        libra_error_t frameErr = libra.d3d12_filter_chain_frame(&dx_filterChain, d3d12CmdList, lp.frameCount,
            input, output, &viewport, NULL, NULL);

        // D3D12 needs an extra reminder or imgui gets hidden by fx
        lp.commandList->setFramebuffer(lp.outputFramebuffer);
#endif

#if LIBRA_RUNTIME_VULKAN
        auto* vkCmdList = static_cast<plume::VulkanCommandList*>(lp.commandList)->vk;
        auto* vkInput = static_cast<plume::VulkanTexture*>(lp.inputTexture);
        auto* vkOutput = static_cast<plume::VulkanTexture*>(lp.outputTexture);

        libra_image_vk_t input = {
            .handle = vkInput->vk,
            .format = vkInput->imageFormat,
            .width = vkInput->desc.width,
            .height = vkInput->desc.height
        };

        libra_image_vk_t output = {
            .handle = vkOutput->vk,
            // todo: why is swapchain texture format being lost? reusing vkinput for now
            .format = vkInput->imageFormat,
            .width = vkOutput->desc.width,
            .height = vkOutput->desc.height
        };

        lp.commandList->begin();

        libra_error_t frameErr = libra.vk_filter_chain_frame(&vk_filterChain, vkCmdList, lp.frameCount,
                                                                input, output, NULL, NULL, NULL);
#endif
    }

    bool Librashader::ready() {
        // todo: Check config and check appropriate filterchain
#ifdef LIBRA_RUNTIME_D3D12
        return dx_filterChain;
#elif LIBRA_RUNTIME_VULKAN
        return vk_filterChain; 
#endif // todo: metal
    }

    void Librashader::reset() {
#ifdef LIBRA_RUNTIME_D3D12
        if (dx_filterChain) {
            libra.d3d12_filter_chain_free(&dx_filterChain);
        }
#endif

#ifdef LIBRA_RUNTIME_VULKAN
        if (vk_filterChain) {
            libra.vk_filter_chain_free(&vk_filterChain);
        }
#endif

        if (preset) {
            libra.preset_free(&preset);
            preset = nullptr;
        }

        if (libraContext) {
            libra.preset_ctx_free(&libraContext);
        }

        currentRuntimeParams.clear();
        currentShaderPath.clear();
    }

    bool Librashader::setup(RenderDevice *device, std::string path) {
        reset();

        libra.preset_ctx_create(&libraContext);

        libra_preset_opt_t opts;
        opts.original_aspect_uniforms = true;

        libra_error_t err = libra.preset_create_with_options(path.c_str(), &libraContext, &opts, &preset);
        // todo: error checks for preset create and get runtime params
        //libra.preset_print(&preset);

        if (err)
            return false;

        // Build out parameter vector
        currentRuntimeParams.clear();
        libra_preset_param_list_t preset_parameters;
        err = libra.preset_get_runtime_params(&preset, &preset_parameters);

        if (err)
            return false;

        for (int i = 0; i < preset_parameters.length; i++) {
            libra_preset_param_t param = preset_parameters.parameters[i];
            currentRuntimeParams.emplace_back(
                param.name,
                param.description,
                param.initial,
                param.minimum,
                param.maximum,
                param.step
            );
        }
        libra.preset_free_runtime_params(preset_parameters);

#ifdef LIBRA_RUNTIME_D3D12
        auto* d3d12Device = static_cast<plume::D3D12Device*>(device);

        const filter_chain_d3d12_opt_t filterOptions = {
            LIBRASHADER_CURRENT_VERSION,
            false, // Force use of hlsl
            false, // Force disable mipmaps
            false  // Disable cache
        };

        err = libra.d3d12_filter_chain_create(&preset, d3d12Device->d3d,
                                                            &filterOptions,
                                                            &dx_filterChain);
#elif LIBRA_RUNTIME_VULKAN
        auto* interfaceDevice = static_cast<plume::VulkanDevice*>(device);

        const libra_device_vk_t vkLibraDevice = {
            .physical_device = interfaceDevice->physicalDevice,
            .instance = interfaceDevice->renderInterface->instance,
            .device = interfaceDevice->vk,
            .queue = nullptr, // todo: specify?
            .entry = vkGetInstanceProcAddr
        };

        const filter_chain_vk_opt_t vkFilterChainOpts = {
            .version = LIBRASHADER_CURRENT_VERSION,
            .frames_in_flight = 0,
            .force_no_mipmaps = false,
            .use_dynamic_rendering = false, // recommended to enable but requires 1.3 ext
            .disable_cache = false
        };

        err = libra.vk_filter_chain_create(&preset, vkLibraDevice, &vkFilterChainOpts, &vk_filterChain);
#endif // todo: metal

        if (!err)
            currentShaderPath = path;

        return !err;
    }
}
