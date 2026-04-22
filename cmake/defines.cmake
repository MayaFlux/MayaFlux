add_compile_definitions(MAYAFLUX_DEVELOPMENT)

if(WIN32)
    add_compile_definitions(MAYAFLUX_PLATFORM_WINDOWS)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
    add_compile_definitions(_USE_MATH_DEFINES)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
elseif(APPLE)
    add_compile_definitions(MAYAFLUX_PLATFORM_MACOS)
elseif(UNIX)
    add_compile_definitions(MAYAFLUX_PLATFORM_LINUX)
else()
    add_compile_definitions(MAYAFLUX_PLATFORM_UNKNOWN)
endif()
add_compile_definitions(RTAUDIO_BACKEND GLFW_BACKEND)

if(APPLE)
    message(STATUS
            "Using system Clang on macOS (minimum macOS 14 required for C++23)")
endif()

if(WIN32)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd" CACHE STRING "" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR
    ("${CMAKE_CONFIGURATION_TYPES}" MATCHES "Debug" AND NOT CMAKE_BUILD_TYPE ))
    add_compile_definitions(MAYAFLUX_DEBUG=1)
    message(STATUS "Debug build detected - MAYAFLUX_DEBUG enabled")
else()
    add_compile_definitions(MAYAFLUX_DEBUG=0)
    message(STATUS "Release build - MAYAFLUX_DEBUG disabled")
endif()

set(DATA_DIR "${CMAKE_SOURCE_DIR}/data")

if(WIN32)
    set(CMAKE_INSTALL_PREFIX "C:/MayaFlux" CACHE PATH "Installation directory"
        FORCE)
    message(STATUS "Setting Windows install prefix to: ${CMAKE_INSTALL_PREFIX}")
endif()

set(USER_PROJECT_FILE "${CMAKE_SOURCE_DIR}/src/user_project.hpp")
if(NOT EXISTS ${USER_PROJECT_FILE})
    message(STATUS "Creating user_project.hpp from template...")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/user_project.hpp.in
        ${USER_PROJECT_FILE}
        @ONLY
    )
endif()
set(USER_SOURCES ${USER_PROJECT_FILE})

option(MAYAFLUX_BUILD_TESTS "Build MayaFlux test suite" OFF)
option(MAYAFLUX_PORTABLE "Build portable binaries with RPATH" OFF)
option(MAYAFLUX_DEV "Build MayaFlux for development with debug symobls, rpath and tests" OFF)
option(MAYAFLUX_SHIP_DEV "Build MayaFlux for dev build shipping with debug symbols" OFF)
option(MAYAFLUX_SHIP_REL "Build MayaFlux for shipping with optimizations and no debug symbols" OFF)

if(MAYAFLUX_DEV)
    message(STATUS "Development mode enabled")
    set(MAYAFLUX_BUILD_TESTS ON CACHE BOOL "" FORCE)
    set(MAYAFLUX_PORTABLE ON CACHE BOOL "" FORCE)
    set(CMAKE_BUILD_TYPE DEBUG)
    set(EXAMPLE_SHADER_DIR "${CMAKE_SOURCE_DIR}/src/examples/shaders")
    include(examples_shaders)
else()
    set(EXAMPLE_SHADER_DIR "")
endif()

if(MAYAFLUX_SHIP_DEV)
    message(STATUS "Shipping mode enabled: Dev")
    set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
    set(MAYAFLUX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(EXAMPLE_SHADER_DIR "")
endif()

if(MAYAFLUX_SHIP_REL)
    message(STATUS "Shipping mode enabled: Release")
    set(CMAKE_BUILD_TYPE RELEASE)
    set(MAYAFLUX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(EXAMPLE_SHADER_DIR "")
endif()

add_compile_definitions(ASIO_STANDALONE)
