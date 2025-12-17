cmake_minimum_required(VERSION 3.20)
project(rt64 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if (APPLE)
    enable_language(OBJC OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
endif()

option(RT64_STATIC "Build RT64 as a static library" OFF)
option(RT64_SDL_WINDOW_VULKAN "Build RT64 to expect an SDL Window outside of Windows" OFF)

if (NOT ${RT64_STATIC})
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if (WIN32)
    add_compile_definitions(NOMINMAX)
endif()

set(ZSTD_LEGACY_SUPPORT OFF)
set(ZSTD_BUILD_STATIC ON)
set(ZSTD_BUILD_SHARED OFF)

add_subdirectory(src/contrib/re-spirv)
add_subdirectory(src/contrib/nativefiledialog-extended)
add_subdirectory(src/contrib/zstd/build/cmake)
add_subdirectory(src/contrib/plume)

# Include plume's shader compilation infrastructure
include("${PROJECT_SOURCE_DIR}/src/contrib/plume/cmake/PlumeShaders.cmake")
plume_shaders_init()

# RT64 shader compilation settings
set(RT64_SHADER_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/src")
set(RT64_SHADER_MODEL "6_3")
set(RT64_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/src/shaders")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Use /W4 for clang-cl, as -Wall maps to MSVC's /Wall which in turn maps to clang's -Weverything
    if (CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra)
    endif()
    add_compile_options(-Wno-nullability-completeness -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-missing-field-initializers -Wno-switch -Wno-tautological-undefined-compare)
    # imgui currently triggers some compiler warnings on clang 19 for function casts which leads to a failed build, so disable that warning on clang 19+
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.0)
        add_compile_options(-Wno-cast-function-type-mismatch)
    endif()
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 20.0)
        add_compile_options(-Wno-deprecated-literal-operator -Wno-nontrivial-memcall)
    endif()
endif()

set(ANDROID_PLATFORM android-24)
set(ANDROID_ABI arm64-v8a)

# Point cmake at src/contrib/mupen64plus-win32-deps/SDL2-2.26.3 for SDL2 on windows, look for an installed package on other systems
if (WIN32)
    set(SDL2_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/include")
    set(SDL2_LIBRARIES "SDL2" "SDL2main")
    link_directories("${PROJECT_SOURCE_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64")
else()
    find_package(SDL2 REQUIRED)
endif()

message(STATUS "${SDL2_INCLUDE_DIRS} ${SDL2_LIBRARIES}")

add_compile_definitions(
    HLSL_CPU
    FFX_GCC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND RT64_SDL_WINDOW_VULKAN)
    add_compile_definitions("SDL_VULKAN_ENABLED")
    add_compile_definitions("RT64_SDL_WINDOW_VULKAN")
endif()

set (SOURCES
    "${PROJECT_SOURCE_DIR}/src/common/rt64_common.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_dynamic_libraries.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_elapsed_timer.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_emulator_configuration.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_enhancement_configuration.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_filesystem_zip.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_load_types.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_mapped_file.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_math.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_profiling_timer.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_replacement_database.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_thread.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_timer.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_user_configuration.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_user_paths.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_f3d.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_f3dex.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_f3dex2.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_f3dgolden.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_f3dpd.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_f3dwave.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_f3dzex2.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_l3dex2.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_rdp.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_s2dex.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_s2dex2.cpp"
    "${PROJECT_SOURCE_DIR}/src/gbi/rt64_gbi_extended.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/rt64_camera_controller.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/rt64_debugger_inspector.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/rt64_file_dialog.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/rt64_inspector.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_application.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_application_window.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_color_converter.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_command_warning.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_draw_call.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_framebuffer.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_framebuffer_changes.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_framebuffer_manager.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_framebuffer_pair.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_framebuffer_storage.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_game_frame.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_interpreter.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_light_manager.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_present_queue.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_projection.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_rdp.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_rdp_tmem.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_rigid_body.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_rsp.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_state.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_vi.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_workload.cpp"
    "${PROJECT_SOURCE_DIR}/src/hle/rt64_workload_queue.cpp"
    "${PROJECT_SOURCE_DIR}/src/imgui/imgui_impl_sdl2_custom.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_buffer_uploader.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_framebuffer_renderer.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_geometry_mode.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_native_target.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_optimus.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_projection_processor.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_raster_shader.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_raster_shader_cache.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_render_target.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_render_target_manager.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_render_worker.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_rsp_processor.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_shader_common.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_shader_compiler.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_shader_library.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_texture_cache.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_tile_processor.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_transform_processor.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_upscaler.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_vertex_processor.cpp"
    "${PROJECT_SOURCE_DIR}/src/render/rt64_vi_renderer.cpp"

    "${PROJECT_SOURCE_DIR}/src/preset/rt64_preset.cpp"
    "${PROJECT_SOURCE_DIR}/src/preset/rt64_preset_draw_call.cpp"
    "${PROJECT_SOURCE_DIR}/src/preset/rt64_preset_light.cpp"
    "${PROJECT_SOURCE_DIR}/src/preset/rt64_preset_material.cpp"
    "${PROJECT_SOURCE_DIR}/src/preset/rt64_preset_scene.cpp"

    "${PROJECT_SOURCE_DIR}/src/shared/rt64_hlsl_json.cpp"

    "${PROJECT_SOURCE_DIR}/src/rhi/rt64_render_hooks.cpp"

    "${PROJECT_SOURCE_DIR}/src/contrib/imgui/imgui.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/imgui/imgui_demo.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/imgui/imgui_draw.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/imgui/imgui_tables.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/imgui/imgui_widgets.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/imgui/backends/imgui_impl_vulkan.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/im3d/im3d.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/implot/implot.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/implot/implot_demo.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/implot/implot_items.cpp"
    "${PROJECT_SOURCE_DIR}/src/contrib/miniz/miniz.c"
)

include_directories(
    "${SDL2_INCLUDE_DIRS}"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/contrib"
    "${PROJECT_SOURCE_DIR}/src/contrib/plume"
    "${PROJECT_SOURCE_DIR}/src/contrib/imgui"
    "${PROJECT_SOURCE_DIR}/src/contrib/hlslpp/include"
    "${PROJECT_SOURCE_DIR}/src/contrib/mupen64plus-core/src/api"
    "${PROJECT_SOURCE_DIR}/src/contrib/nativefiledialog-extended/src/include"
)

if (WIN32)
    # DXC only needs to be included during runtime for Windows.
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/dxc/inc")
endif()

if (${RT64_STATIC})
    add_library(rt64 STATIC ${SOURCES})
else()
    add_library(rt64 SHARED ${SOURCES})
endif()

set_target_properties(rt64 PROPERTIES OUTPUT_NAME "rt64")
set_target_properties(rt64 PROPERTIES PREFIX "")

# Add common libraries.
target_link_libraries(rt64 re-spirv)
target_link_libraries(rt64 nfd)
target_link_libraries(rt64 libzstd_static)
target_link_libraries(rt64 plume)

# Add tools.
add_subdirectory(src/tools/texture_hasher)
add_subdirectory(src/tools/texture_packer)

# Add any Apple-specific source files and libraries
if (APPLE)
    target_sources(rt64 PRIVATE "${PROJECT_SOURCE_DIR}/src/apple/rt64_apple.mm")
    target_include_directories(rt64 PRIVATE "${PROJECT_SOURCE_DIR}/src/contrib/metal-cpp")
    target_link_libraries(rt64 "-framework Metal -framework QuartzCore -framework CoreGraphics -framework IOKit")
endif()

# Add any Windows-specific source files and libraries
if (WIN32)
    target_sources(rt64 PRIVATE
        "${PROJECT_SOURCE_DIR}/src/contrib/imgui/backends/imgui_impl_dx12.cpp"
        "${PROJECT_SOURCE_DIR}/src/contrib/imgui/backends/imgui_impl_win32.cpp"
    )
    target_link_libraries(rt64
        delayimp.lib
        D3D12
        DXGI
        Shcore.lib
        ${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64/dxcompiler.lib)

    target_include_directories(rt64 PRIVATE ${zstd_SOURCE_DIR}/lib)
endif()

target_link_directories(rt64 PRIVATE ${PROJECT_SOURCE_DIR}/src/contrib/dxc)

if (NOT ANDROID)
    target_link_libraries(rt64 ${SDL2_LIBRARIES})
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    if (NOT RT64_SDL_WINDOW_VULKAN)
        find_package(X11 REQUIRED)
        target_include_directories(rt64 PUBLIC ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})
        target_link_libraries(rt64 ${X11_LIBRARIES} ${X11_Xrandr_LIB})
    endif()
endif()

# ============================================================================
# Shader compilation using plume's infrastructure
# ============================================================================

plume_preprocess_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RenderParams.hlsli" "RenderParams.hlsli" VAR_NAME "RenderParams"
    INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

if (WIN32)
    plume_compile_library_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSLibrary"
        INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
    plume_compile_library_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSLibraryMS"
        INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D MULTISAMPLING OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
    plume_compile_library_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterVS.hlsl" "RasterVSLibrary"
        INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
endif()

plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSDynamic" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D DYNAMIC_RENDER_PARAMS OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSDynamicMS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D DYNAMIC_RENDER_PARAMS -D MULTISAMPLING OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSSpecConstant" "PSMain"
    SPEC_CONSTANTS SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D SPEC_CONSTANT_RENDER_PARAMS OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSSpecConstantMS" "PSMain"
    SPEC_CONSTANTS SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D SPEC_CONSTANT_RENDER_PARAMS -D MULTISAMPLING OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSSpecConstantFlat" "PSMain"
    SPEC_CONSTANTS SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D SPEC_CONSTANT_RENDER_PARAMS -D VERTEX_FLAT_COLOR OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterPS.hlsl" "RasterPSSpecConstantFlatMS" "PSMain"
    SPEC_CONSTANTS SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D SPEC_CONSTANT_RENDER_PARAMS -D VERTEX_FLAT_COLOR -D MULTISAMPLING OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_vertex_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterVS.hlsl" "RasterVSDynamic" "VSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D DYNAMIC_RENDER_PARAMS OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_vertex_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterVS.hlsl" "RasterVSSpecConstant" "VSMain"
    SPEC_CONSTANTS SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D SPEC_CONSTANT_RENDER_PARAMS OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_vertex_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RasterVS.hlsl" "RasterVSSpecConstantFlat" "VSMain"
    SPEC_CONSTANTS SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS}
    EXTRA_ARGS -D SPEC_CONSTANT_RENDER_PARAMS -D VERTEX_FLAT_COLOR OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbChangesClearCS.hlsl" "FbChangesClearCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbChangesDrawColorPS.hlsl" "FbChangesDrawColorPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbChangesDrawDepthPS.hlsl" "FbChangesDrawDepthPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbReadAnyChangesCS.hlsl" "FbReadAnyChangesCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -O0 OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbReinterpretCS.hlsl" "FbReinterpretCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -O0 OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbReadAnyFullCS.hlsl" "FbReadAnyFullCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -O0 OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbWriteColorCS.hlsl" "FbWriteColorCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -O0 OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbWriteDepthCS.hlsl" "FbWriteDepthCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FbWriteDepthCS.hlsl" "FbWriteDepthCSMS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D MULTISAMPLING OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/GaussianFilterRGB3x3CS.hlsl" "GaussianFilterRGB3x3CS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/BoxFilterCS.hlsl" "BoxFilterCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/BicubicScalingCS.hlsl" "BicubicScalingCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/HistogramAverageCS.hlsl" "HistogramAverageCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/HistogramClearCS.hlsl" "HistogramClearCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/HistogramSetCS.hlsl" "HistogramSetCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/IdleCS.hlsl" "IdleCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/LuminanceHistogramCS.hlsl" "LuminanceHistogramCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/PostBlendDitherNoisePS.hlsl" "PostBlendDitherNoiseAddPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D ADD_MODE OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/PostBlendDitherNoisePS.hlsl" "PostBlendDitherNoiseSubPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SUB_MODE OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/PostBlendDitherNoisePS.hlsl" "PostBlendDitherNoiseSubNegativePS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SUB_MODE -D NEGATIVE_MODE OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RSPModifyCS.hlsl" "RSPModifyCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RSPProcessCS.hlsl" "RSPProcessCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RSPWorldCS.hlsl" "RSPWorldCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RSPSmoothNormalCS.hlsl" "RSPSmoothNormalCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RSPVertexTestZCS.hlsl" "RSPVertexTestZCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RSPVertexTestZCS.hlsl" "RSPVertexTestZCSMS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D MULTISAMPLING OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyColorToDepthPS.hlsl" "RtCopyColorToDepthPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyColorToDepthPS.hlsl" "RtCopyColorToDepth2XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_2X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyColorToDepthPS.hlsl" "RtCopyColorToDepth4XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_4X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyColorToDepthPS.hlsl" "RtCopyColorToDepth8XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_8X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyDepthToColorPS.hlsl" "RtCopyDepthToColorPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyDepthToColorPS.hlsl" "RtCopyDepthToColor2XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_2X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyDepthToColorPS.hlsl" "RtCopyDepthToColor4XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_4X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/RtCopyDepthToColorPS.hlsl" "RtCopyDepthToColor8XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_8X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/TextureCopyPS.hlsl" "TextureCopyPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_compute_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/TextureDecodeCS.hlsl" "TextureDecodeCS" "CSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/TextureResolvePS.hlsl" "TextureResolveSamples2XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_2X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/TextureResolvePS.hlsl" "TextureResolveSamples4XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_4X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/TextureResolvePS.hlsl" "TextureResolveSamples8XPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D SAMPLES_8X OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/VideoInterfacePS.hlsl" "VideoInterfacePSRegular" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/VideoInterfacePS.hlsl" "VideoInterfacePSPixel" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} EXTRA_ARGS -D PIXEL_ANTIALIASING OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

plume_compile_vertex_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/FullScreenVS.hlsl" "FullScreenVS" "VSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_vertex_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/Im3DVS.hlsl" "Im3DVS" "VSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/ComposePS.hlsl" "ComposePS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/DebugPS.hlsl" "DebugPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/Im3DPS.hlsl" "Im3DPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})
plume_compile_pixel_shader(rt64 "${PROJECT_SOURCE_DIR}/src/shaders/PostProcessPS.hlsl" "PostProcessPS" "PSMain"
    SHADER_MODEL ${RT64_SHADER_MODEL} INCLUDE_DIRS ${RT64_SHADER_INCLUDE_DIRS} OUTPUT_DIR ${RT64_SHADER_OUTPUT_DIR})

target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/src)

