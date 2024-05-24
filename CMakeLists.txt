cmake_minimum_required(VERSION 3.20)
project(rt64)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

option(RT64_BUILD_EXAMPLES "Build examples for RT64" OFF)
if (${RT64_BUILD_EXAMPLES})
    set(RT64_STATIC ON)
endif()

function(preprocess INFILE OUTFILE OPTIONS)
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            add_custom_command(OUTPUT ${OUTFILE}
                COMMAND clang -x c -E -P ${INFILE} -o ${OUTFILE} ${OPTIONS}
                DEPENDS ${INFILE})
        else()
            add_custom_command(OUTPUT ${OUTFILE}
                COMMAND ${CMAKE_CXX_COMPILER} /Zs /EP ${INFILE} ${OPTIONS} > ${OUTFILE}
                DEPENDS ${INFILE})
        endif()
    else()
        add_custom_command(OUTPUT ${OUTFILE}
            COMMAND ${CMAKE_CXX_COMPILER} -x c -E -P ${INFILE} -o ${OUTFILE} ${OPTIONS}
            DEPENDS ${INFILE})
    endif()
endfunction()

if (WIN32)
    set (DXC "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxc.exe")
    add_compile_definitions(NOMINMAX)

    # Dependencies that must be next to the DLL.
    configure_file("${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxcompiler.dll" "dxcompiler.dll" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxil.dll" "dxil.dll" COPYONLY)
else()
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        if (APPLE)
            set (DXC "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxc")
        else()
            set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxc")
        endif()
    else()
        if (APPLE)
            set (DXC "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/arm64/dxc-macos")
        else()
            set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/arm64/dxc-linux")
        endif()
    endif()

    message(STATUS "DXC: ${DXC}")
endif()

# Build the file_to_c utility for converting binary files into a .c and .h file pair
add_executable(file_to_c src/tools/file_to_c.cpp)

add_subdirectory(src/contrib/nativefiledialog-extended)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-nullability-completeness)
endif()

set(ANDROID_PLATFORM android-24)
set(ANDROID_ABI arm64-v8a)

# For DXC
set (DXC_COMMON_OPTS "-I${PROJECT_SOURCE_DIR}/src")
set (DXC_DXIL_OPTS "-Wno-ignored-attributes")
set (DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set (DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0")
set (DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-fvk-invert-y")
set (DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set (DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
set (DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3" "-fspv-target-env=vulkan1.1spirv1.4" "-fspv-extension=SPV_KHR_ray_tracing" "-fspv-extension=SPV_EXT_descriptor_indexing")

function(build_shader_spirv_impl TARGETOBJ FILENAME TARGET_NAME OUTNAME)
    add_custom_command(OUTPUT ${OUTNAME}.spv
        COMMAND ${DXC} ${DXC_SPV_OPTS} ${ARGN} ${FILENAME} /Fo ${OUTNAME}.spv
        DEPENDS ${FILENAME})
    add_custom_command(OUTPUT ${OUTNAME}.spirv.c
        COMMAND file_to_c ${OUTNAME}.spv ${TARGET_NAME}BlobSPIRV ${OUTNAME}.spirv.c ${OUTNAME}.spirv.h
        DEPENDS ${OUTNAME}.spv file_to_c
        BYPRODUCTS ${OUTNAME}.spirv.h)
    target_sources(${TARGETOBJ} PRIVATE ${OUTNAME}.spirv.c)
endfunction()

function(build_shader_dxil_impl TARGETOBJ FILENAME TARGET_NAME OUTNAME)
    add_custom_command(OUTPUT ${OUTNAME}.dxil
        COMMAND ${DXC} ${DXC_DXIL_OPTS} ${ARGN} ${FILENAME} /Fo ${OUTNAME}.dxil
        DEPENDS ${FILENAME})
    add_custom_command(OUTPUT ${OUTNAME}.dxil.c
        COMMAND file_to_c ${OUTNAME}.dxil ${TARGET_NAME}BlobDXIL ${OUTNAME}.dxil.c ${OUTNAME}.dxil.h
        DEPENDS ${OUTNAME}.dxil file_to_c
        BYPRODUCTS ${OUTNAME}.dxil.h)
    target_sources(${TARGETOBJ} PRIVATE ${OUTNAME}.dxil.c)
endfunction()

function(build_shader TARGETOBJ SHADERNAME OPTIONS)
    set(FILENAME "${PROJECT_SOURCE_DIR}/${SHADERNAME}")
    if (${ARGC} GREATER 3)
        set(OUTNAME "${CMAKE_BINARY_DIR}/${ARGV3}")
    else()
        set(OUTNAME "${CMAKE_BINARY_DIR}/${SHADERNAME}")
    endif()
    # Get any optional compiler args passed to this function
    if (${ARGC} GREATER 4)
        set(EXTRA_ARGS "${ARGN}")
        list(REMOVE_AT EXTRA_ARGS 0)
    endif()
    cmake_path(GET OUTNAME STEM TARGET_NAME)
    cmake_path(GET OUTNAME PARENT_PATH OUTPUT_DIR)
    file(MAKE_DIRECTORY ${OUTPUT_DIR})
    # Compile DXIL shader binaries if building on Windows
    if (WIN32)
        build_shader_dxil_impl(${TARGETOBJ} ${FILENAME} ${TARGET_NAME} ${OUTNAME} ${OPTIONS} ${EXTRA_ARGS})
    endif()
    build_shader_spirv_impl(${TARGETOBJ} ${FILENAME} ${TARGET_NAME} ${OUTNAME} ${OPTIONS} ${EXTRA_ARGS})
endfunction()

function(build_shader_spirv TARGETOBJ SHADERNAME OPTIONS)
    set(FILENAME "${PROJECT_SOURCE_DIR}/${SHADERNAME}")
    if (${ARGC} GREATER 3)
        set(OUTNAME "${CMAKE_BINARY_DIR}/${ARGV3}")
    else()
        set(OUTNAME "${CMAKE_BINARY_DIR}/${SHADERNAME}")
    endif()
    # Get any optional compiler args passed to this function
    if (${ARGC} GREATER 4)
        set(EXTRA_ARGS "${ARGN}")
        list(REMOVE_AT EXTRA_ARGS 0)
    endif()
    cmake_path(GET OUTNAME STEM TARGET_NAME)
    cmake_path(GET OUTNAME PARENT_PATH OUTPUT_DIR)
    file(MAKE_DIRECTORY ${OUTPUT_DIR})
    build_shader_spirv_impl(${TARGETOBJ} ${FILENAME} ${TARGET_NAME} ${OUTNAME} ${OPTIONS} ${EXTRA_ARGS})
endfunction()

function(preprocess_shader TARGETOBJ SHADERNAME)
    set(FILENAME "${PROJECT_SOURCE_DIR}/${SHADERNAME}")
    set(OUTNAME "${CMAKE_BINARY_DIR}/${SHADERNAME}")
    cmake_path(GET SHADERNAME STEM TARGET_NAME)
    cmake_path(GET OUTNAME PARENT_PATH OUTPUT_DIR)
    preprocess(${FILENAME} ${OUTNAME}.rw ${DXC_COMMON_OPTS})
    add_custom_command(OUTPUT ${OUTNAME}.rw.c
        COMMAND file_to_c ${OUTNAME}.rw ${TARGET_NAME}Text ${OUTNAME}.rw.c ${OUTNAME}.rw.h
        DEPENDS ${OUTNAME}.rw file_to_c
        BYPRODUCTS ${OUTNAME}.rw.h)
    file(MAKE_DIRECTORY ${OUTPUT_DIR})
    target_sources(${TARGETOBJ} PRIVATE ${OUTNAME}.rw.c)
endfunction()

function(build_pixel_shader TARGETOBJ SHADERNAME)
    build_shader(${TARGETOBJ} ${SHADERNAME} "${DXC_PS_OPTS}" ${ARGN})
endfunction()

function(build_vertex_shader TARGETOBJ SHADERNAME)
    build_shader(${TARGETOBJ} ${SHADERNAME} "${DXC_VS_OPTS}" ${ARGN})
endfunction()

function(build_pixel_shader_spirv TARGETOBJ SHADERNAME)
    build_shader_spirv(${TARGETOBJ} ${SHADERNAME} "${DXC_PS_OPTS}" ${ARGN})
endfunction()

function(build_vertex_shader_spirv TARGETOBJ SHADERNAME)
    build_shader_spirv(${TARGETOBJ} ${SHADERNAME} "${DXC_VS_OPTS}" ${ARGN})
endfunction()

function(build_compute_shader TARGETOBJ SHADERNAME)
    build_shader(${TARGETOBJ} ${SHADERNAME} "${DXC_CS_OPTS}" ${ARGN})
endfunction()

function(build_geo_shader TARGETOBJ SHADERNAME)
    build_shader(${TARGETOBJ} ${SHADERNAME} "${DXC_GS_OPTS}" ${ARGN})
endfunction()

function(build_ray_shader TARGETOBJ SHADERNAME)
    build_shader(${TARGETOBJ} ${SHADERNAME} "${DXC_RT_OPTS}" ${ARGN})
endfunction()

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

set (SOURCES
    "${PROJECT_SOURCE_DIR}/src/common/rt64_common.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_dynamic_libraries.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_elapsed_timer.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_emulator_configuration.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_enhancement_configuration.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_math.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/rt64_profiling_timer.cpp"
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
    "${PROJECT_SOURCE_DIR}/src/vulkan/rt64_vulkan.cpp"

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
)

include_directories(
    "${SDL2_INCLUDE_DIRS}"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/contrib"
    "${PROJECT_SOURCE_DIR}/src/contrib/D3D12MemoryAllocator/include"
    "${PROJECT_SOURCE_DIR}/src/contrib/D3D12MemoryAllocator/src"
    "${PROJECT_SOURCE_DIR}/src/contrib/imgui"
    "${PROJECT_SOURCE_DIR}/src/contrib/hlslpp/include"
    "${PROJECT_SOURCE_DIR}/src/contrib/Vulkan-Headers/include"
    "${PROJECT_SOURCE_DIR}/src/contrib/VulkanMemoryAllocator/include"
    "${PROJECT_SOURCE_DIR}/src/contrib/mupen64plus-core/src/api"
    "${PROJECT_SOURCE_DIR}/src/contrib/nativefiledialog-extended/src/include"
)

if (WIN32)
    # DXC only needs to be included during runtime for Windows.
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/dxc/inc")
endif()

option(RT64_STATIC "Build RT64 as a static library" OFF)
if (${RT64_STATIC})
    add_library(rt64 STATIC ${SOURCES})
else()
    add_library(rt64 SHARED ${SOURCES})
endif()

set_target_properties(rt64 PROPERTIES OUTPUT_NAME "rt64")
set_target_properties(rt64 PROPERTIES PREFIX "")

# Add common libraries.
target_link_libraries(rt64 nfd)

# Add any Windows-specific source files and libraries
if (WIN32)
    target_sources(rt64 PRIVATE
        "${PROJECT_SOURCE_DIR}/src/d3d12/rt64_d3d12.cpp"
        "${PROJECT_SOURCE_DIR}/src/contrib/imgui/backends/imgui_impl_dx12.cpp"
        "${PROJECT_SOURCE_DIR}/src/contrib/imgui/backends/imgui_impl_win32.cpp"
    )
    target_link_libraries(rt64
        delayimp.lib
        D3D12
        DXGI
        Shcore.lib
        ${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64/dxcompiler.lib)
endif()

target_link_directories(rt64 PRIVATE ${PROJECT_SOURCE_DIR}/src/contrib/dxc)

if (NOT ANDROID)
    target_link_libraries(rt64 ${SDL2_LIBRARIES})
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(X11 REQUIRED)
    target_include_directories(rt64 PUBLIC ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})
    target_link_libraries(rt64 ${X11_LIBRARIES} ${X11_Xrandr_LIB})
endif()

preprocess_shader(rt64 "src/shaders/RasterPS.hlsl")
preprocess_shader(rt64 "src/shaders/RasterVS.hlsl")

build_pixel_shader(  rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSDynamic.hlsl" "-D DYNAMIC_RENDER_PARAMS")
build_pixel_shader(  rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSDynamicMS.hlsl" "-D DYNAMIC_RENDER_PARAMS" "-D MULTISAMPLING")
build_pixel_shader_spirv(rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSSpecConstant.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS")
build_pixel_shader_spirv(rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSSpecConstantFlat.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS" "-D VERTEX_FLAT_COLOR")
build_pixel_shader_spirv(rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSSpecConstantDepth.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS" "-D OUTPUT_DEPTH")
build_pixel_shader_spirv(rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSSpecConstantDepthMS.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS" "-D OUTPUT_DEPTH" "-D MULTISAMPLING")
build_pixel_shader_spirv(rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSSpecConstantFlatDepth.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS" "-D VERTEX_FLAT_COLOR" "-D OUTPUT_DEPTH")
build_pixel_shader_spirv(rt64 "src/shaders/RasterPS.hlsl" "src/shaders/RasterPSSpecConstantFlatDepthMS.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS" "-D VERTEX_FLAT_COLOR" "-D OUTPUT_DEPTH" "-D MULTISAMPLING")
build_vertex_shader( rt64 "src/shaders/RasterVS.hlsl" "src/shaders/RasterVSDynamic.hlsl" "-D DYNAMIC_RENDER_PARAMS")
build_vertex_shader_spirv( rt64 "src/shaders/RasterVS.hlsl" "src/shaders/RasterVSSpecConstant.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS")
build_vertex_shader_spirv( rt64 "src/shaders/RasterVS.hlsl" "src/shaders/RasterVSSpecConstantFlat.hlsl" "-D SPEC_CONSTANT_RENDER_PARAMS" "-D VERTEX_FLAT_COLOR")
build_compute_shader(rt64 "src/shaders/FbChangesClearCS.hlsl")
build_pixel_shader(  rt64 "src/shaders/FbChangesDrawColorPS.hlsl")
build_pixel_shader(  rt64 "src/shaders/FbChangesDrawDepthPS.hlsl")
build_compute_shader(rt64 "src/shaders/FbReadAnyChangesCS.hlsl" "src/shaders/FbReadAnyChangesCS.hlsl" "-O0")
build_compute_shader(rt64 "src/shaders/FbReinterpretCS.hlsl" "src/shaders/FbReinterpretCS.hlsl" "-O0")
build_compute_shader(rt64 "src/shaders/FbReadAnyFullCS.hlsl" "src/shaders/FbReadAnyFullCS.hlsl" "-O0")
build_compute_shader(rt64 "src/shaders/FbWriteColorCS.hlsl" "src/shaders/FbWriteColorCS.hlsl" "-O0")
build_compute_shader(rt64 "src/shaders/FbWriteDepthCS.hlsl")
build_compute_shader(rt64 "src/shaders/FbWriteDepthCS.hlsl" "src/shaders/FbWriteDepthCSMS.hlsl" "-D MULTISAMPLING")
build_compute_shader(rt64 "src/shaders/GaussianFilterRGB3x3CS.hlsl")
build_compute_shader(rt64 "src/shaders/BoxFilterCS.hlsl")
build_compute_shader(rt64 "src/shaders/BicubicScalingCS.hlsl")
build_compute_shader(rt64 "src/shaders/HistogramAverageCS.hlsl")
build_compute_shader(rt64 "src/shaders/HistogramClearCS.hlsl")
build_compute_shader(rt64 "src/shaders/HistogramSetCS.hlsl")
build_compute_shader(rt64 "src/shaders/IdleCS.hlsl")
build_compute_shader(rt64 "src/shaders/LuminanceHistogramCS.hlsl")
build_pixel_shader(  rt64 "src/shaders/PostBlendDitherNoisePS.hlsl" "src/shaders/PostBlendDitherNoiseAddPS.hlsl" "-D ADD_MODE")
build_pixel_shader(  rt64 "src/shaders/PostBlendDitherNoisePS.hlsl" "src/shaders/PostBlendDitherNoiseSubPS.hlsl" "-D SUB_MODE")
build_compute_shader(rt64 "src/shaders/RSPModifyCS.hlsl")
build_compute_shader(rt64 "src/shaders/RSPProcessCS.hlsl")
build_compute_shader(rt64 "src/shaders/RSPWorldCS.hlsl")
build_compute_shader(rt64 "src/shaders/RSPSmoothNormalCS.hlsl")
build_compute_shader(rt64 "src/shaders/RSPVertexTestZCS.hlsl")
build_compute_shader(rt64 "src/shaders/RSPVertexTestZCS.hlsl" "src/shaders/RSPVertexTestZCSMS.hlsl" "-D MULTISAMPLING")
build_pixel_shader(  rt64 "src/shaders/RtCopyColorToDepthPS.hlsl")
build_pixel_shader(  rt64 "src/shaders/RtCopyColorToDepthPS.hlsl" "src/shaders/RtCopyColorToDepthPSMS.hlsl" "-D MULTISAMPLING")
build_pixel_shader(  rt64 "src/shaders/RtCopyDepthToColorPS.hlsl")
build_pixel_shader(  rt64 "src/shaders/RtCopyDepthToColorPS.hlsl" "src/shaders/RtCopyDepthToColorPSMS.hlsl" "-D MULTISAMPLING")
build_pixel_shader(  rt64 "src/shaders/TextureCopyPS.hlsl")
build_compute_shader(rt64 "src/shaders/TextureDecodeCS.hlsl")
build_pixel_shader(  rt64 "src/shaders/VideoInterfacePS.hlsl" "src/shaders/VideoInterfacePSRegular.hlsl")
build_pixel_shader(  rt64 "src/shaders/VideoInterfacePS.hlsl" "src/shaders/VideoInterfacePSPixel.hlsl" "-D PIXEL_ANTIALIASING")
build_vertex_shader( rt64 "src/shaders/FullScreenVS.hlsl")
build_vertex_shader( rt64 "src/shaders/Im3DVS.hlsl")
build_geo_shader(    rt64 "src/shaders/Im3DGSPoints.hlsl")
build_geo_shader(    rt64 "src/shaders/Im3DGSLines.hlsl")
build_pixel_shader(  rt64 "src/shaders/ComposePS.hlsl")
build_pixel_shader(  rt64 "src/shaders/DebugPS.hlsl")
build_pixel_shader(  rt64 "src/shaders/Im3DPS.hlsl")
build_pixel_shader(  rt64 "src/shaders/PostProcessPS.hlsl")

target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/src)

if (RT64_BUILD_EXAMPLES)
    add_executable(rhi_test "examples/rt64_render_interface.cpp" "examples/rhi_test.cpp")
    target_link_libraries(rhi_test rt64)

    build_pixel_shader(  rhi_test "examples/shaders/RenderInterfaceTestPS.hlsl")
    build_vertex_shader( rhi_test "examples/shaders/RenderInterfaceTestVS.hlsl")
    build_compute_shader(rhi_test "examples/shaders/RenderInterfaceTestCS.hlsl")
    build_ray_shader(    rhi_test "examples/shaders/RenderInterfaceTestRT.hlsl")
    build_pixel_shader(  rhi_test "examples/shaders/RenderInterfaceTestPostPS.hlsl")
    build_vertex_shader( rhi_test "examples/shaders/RenderInterfaceTestPostVS.hlsl")

    target_include_directories(rhi_test PRIVATE ${CMAKE_BINARY_DIR}/examples)
endif()
