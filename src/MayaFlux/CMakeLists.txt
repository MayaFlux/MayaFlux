file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(MayaFluxLib SHARED ${SOURCES} ${HEADERS})

set_target_properties(MayaFluxLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(MayaFluxLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # Points to src/ for #include "MayaFlux/..."
)
target_precompile_headers(MayaFluxLib PUBLIC ../../cmake/pch.h)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(MayaFluxLib PRIVATE GLFW_EXPOSE_NATIVE_WAYLAND)
endif()

if(WIN32)
    find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h)
    if(NOT FFMPEG_INCLUDE_DIR)
        message(FATAL_ERROR "FFmpeg headers not found. Make sure FFmpeg is installed via vcpkg.")
    endif()

    find_library(AVCODEC_LIB avcodec REQUIRED)
    find_library(AVFORMAT_LIB avformat REQUIRED)
    find_library(AVUTIL_LIB avutil REQUIRED)
    find_library(SWRESAMPLE_LIB swresample REQUIRED)
    find_library(SWSCALE_LIB swscale REQUIRED)

    target_include_directories(MayaFluxLib PUBLIC ${FFMPEG_INCLUDE_DIR})
    add_compile_definitions(USE_PPL_BACKEND)

    target_link_libraries(MayaFluxLib PUBLIC
        RtAudio::rtaudio
        Eigen3::Eigen
        glfw
        Vulkan::Vulkan
        ${AVCODEC_LIB}
        ${AVFORMAT_LIB}
        ${AVUTIL_LIB}
        ${SWRESAMPLE_LIB}
        ${SWSCALE_LIB}
    )
    target_compile_definitions(MayaFluxLib PRIVATE MAYAFLUX_STATIC)
else()
    if(APPLE)
        target_include_directories(MayaFluxLib PUBLIC ${oneDPL_INCLUDE_DIRS})
        target_link_libraries(MayaFluxLib PRIVATE oneDPL)
        target_link_libraries(MayaFluxLib PUBLIC fmt::fmt)
        message(STATUS "MayaFlux Journal: Using fmt library on macOS")

        target_link_libraries(MayaFluxLib PUBLIC Vulkan::Vulkan)
    else()
        add_compile_definitions(USE_TBB_BACKEND)
        target_link_libraries(MayaFluxLib PUBLIC PkgConfig::Vulkan)
    endif()

    target_link_libraries(MayaFluxLib PUBLIC TBB::tbb)

    target_link_libraries(MayaFluxLib PUBLIC
        PkgConfig::RtAudio
        PkgConfig::Eigen
        PkgConfig::Glfw
        PkgConfig::LIBAVCODEC
        PkgConfig::LIBAVFORMAT
        PkgConfig::LIBAVUTIL
        PkgConfig::LIBSWRESAMPLE
        PkgConfig::LIBSWSCALE
    )
endif()
