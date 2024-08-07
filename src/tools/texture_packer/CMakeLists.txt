cmake_minimum_required(VERSION 3.20)
project(texture_packer)
set(CMAKE_CXX_STANDARD 17)

add_executable(texture_packer "texture_packer.cpp" "../../contrib/miniz/miniz.c")

target_include_directories(texture_packer PRIVATE "../../contrib")
target_link_libraries(texture_packer PRIVATE libzstd_static)

if (${WIN32})
    target_include_directories(texture_packer PRIVATE ${zstd_SOURCE_DIR}/lib)
endif()