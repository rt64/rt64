cmake_minimum_required(VERSION 3.20)
project(texture_hasher)
set(CMAKE_CXX_STANDARD 17)

include_directories("../../contrib")

add_executable(texture_hasher "texture_hasher.cpp")
