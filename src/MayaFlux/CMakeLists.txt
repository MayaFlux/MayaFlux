file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

if(WIN32)
    add_library(MayaFluxLib STATIC ${SOURCES} ${HEADERS})
else()
    add_library(MayaFluxLib SHARED ${SOURCES} ${HEADERS})
endif()

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
    add_compile_definitions(USE_PPL_BACKEND)

    target_link_libraries(MayaFluxLib PUBLIC
        RtAudio::rtaudio
        Eigen3::Eigen
        glfw
        Vulkan::Vulkan
        FFmpeg::avcodec
        FFmpeg::avformat
        FFmpeg::avutil
        FFmpeg::swresample
        FFmpeg::swscale
        gdi32
        user32
        shell32
        opengl32
    )
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
