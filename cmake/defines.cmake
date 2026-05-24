add_compile_definitions(MAYAFLUX_DEVELOPMENT)

if(WIN32)
    add_compile_definitions(MAYAFLUX_PLATFORM_WINDOWS)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
    add_compile_definitions(_USE_MATH_DEFINES)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
    add_compile_definitions(WASAPI_BACKEND)
    set(MAYAFLUX_WINDOWING_BACKEND "WIN32_BACKEND" CACHE INTERNAL "")
    add_compile_definitions(WIN32_BACKEND)
    set(MAYAFLUX_AUDIO_BACKEND "WASAPI_BACKEND" CACHE INTERNAL "")
    set(MAYAFLUX_MIDI_BACKEND "WINMM_BACKEND" CACHE INTERNAL "")
elseif(APPLE)
    add_compile_definitions(MAYAFLUX_PLATFORM_MACOS)
    add_compile_definitions(RTAUDIO_BACKEND)
    add_compile_definitions(RTMIDI_BACKEND)
    add_compile_definitions(GLFW_BACKEND)
    set(MAYAFLUX_WINDOWING_BACKEND "GLFW_BACKEND" CACHE INTERNAL "")
    set(MAYAFLUX_AUDIO_BACKEND "RTAUDIO_BACKEND" CACHE INTERNAL "")
    set(MAYAFLUX_MIDI_BACKEND "RTMIDI_BACKEND" CACHE INTERNAL "")
elseif(UNIX)
    add_compile_definitions(MAYAFLUX_PLATFORM_LINUX)
    add_compile_definitions(PIPEWIRE_BACKEND)
    add_compile_definitions(WAYLAND_BACKEND)
    set(MAYAFLUX_WINDOWING_BACKEND "WAYLAND_BACKEND" CACHE INTERNAL "")
    set(MAYAFLUX_AUDIO_BACKEND "PIPEWIRE_BACKEND" CACHE INTERNAL "")
    set(MAYAFLUX_MIDI_BACKEND "PIPEWIRE_BACKEND" CACHE INTERNAL "")

else()
    add_compile_definitions(MAYAFLUX_PLATFORM_UNKNOWN)
endif()

if(APPLE)
    message(STATUS
            "Using system Clang on macOS (minimum macOS 14 required for C++23)")
endif()

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
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
set(EXAMPLE_SHADER_DIR "" CACHE PATH "Path to example shaders")

option(MAYAFLUX_BUILD_TESTS "Build MayaFlux test suite" OFF)
option(MAYAFLUX_PORTABLE "Build portable binaries with RPATH" OFF)
option(MAYAFLUX_DEV
       "Build MayaFlux for development with debug symobls, rpath and tests" OFF)
option(MAYAFLUX_BUILD_PROJECT "Build project_launcher binary" OFF)

if(EXAMPLE_SHADER_DIR)
    include(examples_shaders)
endif()

add_compile_definitions(ASIO_STANDALONE)
