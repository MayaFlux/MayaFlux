cmake_minimum_required(VERSION 3.20)
project(spirv-reflect-isolated LANGUAGES C)

# Fetch SPIRV-Reflect
include(FetchContent)
FetchContent_Declare(
    spirv_reflect
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spirv_reflect)

add_library(spirv-reflect-isolated STATIC
    ${spirv_reflect_SOURCE_DIR}/spirv_reflect.c
)

set_target_properties(spirv-reflect-isolated PROPERTIES
    C_STANDARD 11
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(spirv-reflect-isolated
    PUBLIC ${spirv_reflect_SOURCE_DIR}
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(spirv-reflect-isolated PRIVATE -w -O2)
endif()

target_compile_definitions(spirv-reflect-isolated PUBLIC SPIRV_REFLECT_AVAILABLE)
