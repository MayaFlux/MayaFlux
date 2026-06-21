cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CRITICAL_CMAKE_FILES
    config.h.in
    pch.h
    compiler_config.cmake
    defines.cmake
    rpath.cmake
    dependencies.cmake
    shader.cmake
)

foreach(file ${CRITICAL_CMAKE_FILES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${file}")
        message(FATAL_ERROR "Missing critical cmake file: ${file}")
    endif()
endforeach()

project(MayaFlux VERSION 0.5.0)

include(compiler_config)
include(defines)
include(rpath)
include(dependencies)
include(shader)
include(ide_commands)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    include(wayland_protocols_gen)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h"
    @ONLY
)

add_subdirectory(src)

if(MAYAFLUX_BUILD_TESTS)
    message(STATUS "Building tests (MAYAFLUX_BUILD_TESTS=ON)")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Tests disabled (use -DMAYAFLUX_BUILD_TESTS=ON to enable)")
endif()

include(install)

message(STATUS "MayaFlux configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
