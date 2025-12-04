cmake_minimum_required(VERSION 3.12...3.27)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CRITICAL_CMAKE_FILES
    config.h.in
    pch.h
    compiler_config.cmake
    defines.cmake
    dependencies.cmake
    shader.cmake
)

foreach(file ${CRITICAL_CMAKE_FILES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${file}")
        message(FATAL_ERROR "Missing critical cmake file: ${file}")
    endif()
endforeach()

include(compiler_config)

project(MayaFlux VERSION 0.1.0)

include(defines)
include(dependencies)
include(shader)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h"
    @ONLY
)

add_subdirectory(src)

if(UNIX)
    execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json)

    # enable_testing()
    # add_subdirectory(tests)
endif()

include(install)

message(STATUS "MayaFlux configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
