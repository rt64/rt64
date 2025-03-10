cmake_minimum_required(VERSION 3.20)
project(file_to_c)
set(CMAKE_CXX_STANDARD 17)

add_executable(file_to_c "file_to_c.cpp")

if (APPLE)
    set_target_properties(file_to_c PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../rt64.entitlements
    )
endif()
