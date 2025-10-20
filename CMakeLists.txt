cmake_minimum_required(VERSION 3.12...3.27)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CRITICAL_CMAKE_FILES
    cmake/config.h.in
    cmake/pch.h
    cmake/defines.cmake
    cmake/dependencies.cmake
    cmake/compiler_config.cmake
)

foreach(file ${CRITICAL_CMAKE_FILES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${file}")
        message(WARNING "Missing critical cmake file: ${file}")
    endif()
endforeach()

include(defines)

project(MayaFlux VERSION 0.1.0)

if(WIN32)
    set(CMAKE_INSTALL_PREFIX "C:/MayaFlux" CACHE PATH "Installation directory" FORCE)
    message(STATUS "Setting Windows install prefix to: ${CMAKE_INSTALL_PREFIX}")
endif()

include(dependencies)
include(compiler_config)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h"
    @ONLY
)

set(USER_PROJECT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/user_project.hpp")
if(NOT EXISTS ${USER_PROJECT_FILE})
    message(STATUS "Creating user_project.hpp from template...")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/user_project.hpp.in
        ${USER_PROJECT_FILE}
        @ONLY
    )
endif()
set(USER_SOURCES ${USER_PROJECT_FILE})

add_subdirectory(src)

if(UNIX)
    execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json)

    enable_testing()
    add_subdirectory(tests)
endif()

include(install)

message(STATUS "MayaFlux configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
