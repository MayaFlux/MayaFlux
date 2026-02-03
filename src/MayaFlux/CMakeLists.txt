file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(MayaFluxLib SHARED ${SOURCES} ${HEADERS})
target_sources(MayaFluxLib PRIVATE ${CONFIG_IMPL})

#==============================================================
#               Target Definitions and Properties
#==============================================================

if(WIN32)
    add_compile_definitions(USE_PPL_BACKEND)
    target_compile_definitions(MayaFluxLib PRIVATE MAYAFLUX_EXPORTS)
    if(MSVC)
        target_compile_options(MayaFluxLib PRIVATE /MD)

        if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            target_link_options(MayaFluxLib PRIVATE "/IGNORE:4099")
        endif()
    endif()
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(USE_TBB_BACKEND)
    target_compile_definitions(MayaFluxLib PRIVATE GLFW_EXPOSE_NATIVE_WAYLAND)
endif()

set_target_properties(MayaFluxLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

#==============================================================
#                   Target Includes
#==============================================================

target_include_directories(MayaFluxLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # Points to src/ for #include "MayaFlux/..."
)
target_precompile_headers(MayaFluxLib PUBLIC ${CMAKE_SOURCE_DIR}/cmake/pch.h)

if(TARGET compile_shaders)
    target_include_directories(MayaFluxLib PRIVATE ${SHADER_OUTPUT_DIR})
endif()
target_include_directories(MayaFluxLib PRIVATE ${CMAKE_BINARY_DIR}/include)

if(WIN32)
    target_include_directories(MayaFluxLib PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()
if(APPLE)
    target_include_directories(MayaFluxLib PRIVATE ${STB_INCLUDE_DIR})
    target_include_directories(MayaFluxLib PUBLIC ${oneDPL_INCLUDE_DIRS})
endif()

#==============================================================
#                   Target Links
#==============================================================

target_link_libraries(MayaFluxLib PRIVATE ${SPIRV_CROSS_LIBRARIES})

if(MAYAFLUX_USE_SHADERC)
    if(WIN32)
        if(TARGET Vulkan::shaderc_combined)
            target_link_libraries(MayaFluxLib PRIVATE Vulkan::shaderc_combined)
        elseif(TARGET shaderc_combined)
            target_link_libraries(MayaFluxLib PRIVATE shaderc_combined)
        endif()

    else()
        target_link_libraries(MayaFluxLib PRIVATE PkgConfig::shaderc)
    endif()
    target_compile_definitions(MayaFluxLib PRIVATE MAYAFLUX_USE_SHADERC)
endif()

if(WIN32)
    target_link_libraries(MayaFluxLib PUBLIC
        RtAudio::rtaudio
        Eigen3::Eigen
        glfw
        glm::glm
        Vulkan::Vulkan
        FFmpeg::avcodec
        FFmpeg::avformat
        FFmpeg::avutil
        FFmpeg::swresample
        FFmpeg::swscale
        hidapi::hidapi
        RtMidi::rtmidi
        gdi32
        user32
        shell32
        opengl32
    )
else()
    if(APPLE)
        target_link_libraries(MayaFluxLib PRIVATE oneDPL)
        target_link_libraries(MayaFluxLib PUBLIC fmt::fmt)
        target_link_libraries(MayaFluxLib PUBLIC Vulkan::Vulkan)
        target_link_libraries(MayaFluxLib PRIVATE "-framework CoreFoundation")

        message(STATUS "MayaFlux Journal: Using fmt library on macOS")
    else()
        target_link_libraries(MayaFluxLib PUBLIC PkgConfig::Vulkan)
    endif()

    target_link_libraries(MayaFluxLib PUBLIC
        TBB::tbb
        glm::glm
        PkgConfig::RtAudio
        PkgConfig::Eigen
        PkgConfig::Glfw
        PkgConfig::HIDAPI
        PkgConfig::RtMidi
        PkgConfig::LIBAVCODEC
        PkgConfig::LIBAVFORMAT
        PkgConfig::LIBAVUTIL
        PkgConfig::LIBSWRESAMPLE
        PkgConfig::LIBSWSCALE
    )
endif()
