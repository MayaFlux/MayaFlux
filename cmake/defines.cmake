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

if(WIN32)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE
            "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING
            "Vcpkg toolchain file")

        list(APPEND CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows")
        message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    endif()

    if(NOT DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED VCPKG_ROOT)
        message(WARNING "VCPKG_ROOT not found. Windows users should run setup_windows.bat before configuring CMake.")
        message(STATUS "Alternatively, set CMAKE_TOOLCHAIN_FILE to point to your vcpkg installation.")
    endif()
endif()

if(APPLE)
    message(STATUS "Using system Clang on macOS (minimum macOS 14 required for C++23)")
endif()
