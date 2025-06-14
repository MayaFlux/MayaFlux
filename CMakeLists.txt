cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

project(MayaFlux VERSION 0.1.0)

if(WIN32)
    find_package(RtAudio CONFIG REQUIRED)
    find_package(Eigen3 CONFIG REQUIRED)
    # Delegate FFMPEG to find library
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)
    pkg_check_modules(Eigen REQUIRED IMPORTED_TARGET eigen3)

    pkg_check_modules(LIBAVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(LIBAVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(LIBAVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(LIBSWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
    pkg_check_modules(LIBSWSCALE REQUIRED IMPORTED_TARGET libswscale)
endif()

add_compile_definitions(RTAUDIO_BACKEND)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MayaFlux/version.h
    @ONLY
)

find_package(GTest REQUIRED)
enable_testing()

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX ".*main\\.cpp$")

if(WIN32)
    file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
         ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp)

    add_library(MayaFluxLib ${SOURCES} ${HEADERS})
    add_executable(MayaFlux src/main.cpp ${HEADERS})
else()
    add_library(MayaFluxLib ${SOURCES})
    add_executable(MayaFlux src/main.cpp)
endif()

set_target_properties(MayaFluxLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(MayaFluxLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(WIN32)
    # For vcpkg FFmpeg installation
    # Find include directory
    find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h)
    if(NOT FFMPEG_INCLUDE_DIR)
        message(FATAL_ERROR "FFmpeg headers not found. Make sure FFmpeg is installed via vcpkg.")
    endif()

    # Find individual libraries
    find_library(AVCODEC_LIB avcodec REQUIRED)
    find_library(AVFORMAT_LIB avformat REQUIRED)
    find_library(AVUTIL_LIB avutil REQUIRED)
    find_library(SWRESAMPLE_LIB swresample REQUIRED)
    find_library(SWSCALE_LIB swscale REQUIRED)

    target_include_directories(MayaFluxLib PUBLIC ${FFMPEG_INCLUDE_DIR})

    target_link_libraries(MayaFluxLib PUBLIC
        RtAudio::rtaudio
        Eigen3::Eigen
        ${AVCODEC_LIB}
        ${AVFORMAT_LIB}
        ${AVUTIL_LIB}
        ${SWRESAMPLE_LIB}
        ${SWSCALE_LIB}
    )
    target_compile_definitions(MayaFluxLib PRIVATE MAYAFLUX_STATIC)
else()
    target_link_libraries(MayaFluxLib PUBLIC
        PkgConfig::RtAudio
        PkgConfig::Eigen
        PkgConfig::LIBAVCODEC
        PkgConfig::LIBAVFORMAT
        PkgConfig::LIBAVUTIL
        PkgConfig::LIBSWRESAMPLE
        PkgConfig::LIBSWSCALE
    )
endif()

target_link_libraries(MayaFlux PRIVATE MayaFluxLib)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

add_subdirectory(tests)

if(UNIX)
    execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
)
endif()

install(TARGETS MayaFluxLib DESTINATION lib)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(TARGETS MayaFlux DESTINATION bin)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mayaflux.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc
    @ONLY
)

if(UNIX)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc DESTINATION lib/pkgconfig)
endif()

message(STATUS "MayaFlux configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
