if(WIN32)
    add_compile_definitions(MAYAFLUX_PLATFORM_WINDOWS)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
    add_compile_definitions(_USE_MATH_DEFINES)
elseif(APPLE)
    add_compile_definitions(MAYAFLUX_PLATFORM_MACOS)
elseif(UNIX)
    add_compile_definitions(MAYAFLUX_PLATFORM_LINUX)
else()
    add_compile_definitions(MAYAFLUX_PLATFORM_UNKNOWN)
endif()
add_compile_definitions(RTAUDIO_BACKEND GLFW_BACKEND)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/cmake/dependencies" CACHE PATH
    "Persistent dependencies directory")
set(FETCHCONTENT_QUIET OFF CACHE BOOL "Show FetchContent progress")
include(FetchContent)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Don't update dependencies automatically")
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL
    "Use existing dependencies without network")


if(APPLE)
    message(STATUS "Using system Clang on macOS (minimum macOS 14 required for C++23)")
endif()

set(CONFIG_IMPL ${CMAKE_SOURCE_DIR}/cmake/config.cpp)
