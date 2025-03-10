cmake_minimum_required(VERSION 3.20)
project(spirv_cross_msl)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(spirv_cross_msl "spirv_cross_msl.cpp")

target_include_directories(spirv_cross_msl PRIVATE "../../contrib/spirv-cross")
add_subdirectory(../../contrib/spirv-cross ${CMAKE_BINARY_DIR}/spirv-cross)
target_link_libraries(spirv_cross_msl spirv-cross-core spirv-cross-msl)

if (APPLE)
    set_target_properties(spirv_cross_msl PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../rt64.entitlements
    )
endif()
