set(MAYAFLUX_ALL_SOURCES "")
set(MAYAFLUX_ALL_HEADERS "")

include(API/API.cmake)
include(Buffers/Buffers.cmake)
include(Core/Core.cmake)
include(IO/IO.cmake)
include(Journal/Journal.cmake)
include(Kakshya/Kakshya.cmake)
include(Kinesis/Kinesis.cmake)
include(Kriya/Kriya.cmake)
include(Nodes/Nodes.cmake)
include(Nexus/Nexus.cmake)
include(Portal/Portal.cmake)
include(Registry/Registry.cmake)
include(Transitive/Transitive.cmake)
include(Vruta/Vruta.cmake)
include(Yantra/Yantra.cmake)

list(APPEND MAYAFLUX_ALL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/MayaFlux.hpp")

add_library(MayaFluxLib SHARED ${MAYAFLUX_ALL_SOURCES} ${MAYAFLUX_ALL_HEADERS})

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

    mf_add_wayland_protocol(MayaFluxLib
        PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
        BASENAME "xdg-shell"
    )

    mf_add_wayland_protocol(MayaFluxLib
        PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
        BASENAME "xdg-decoration-unstable-v1"
    )

    mf_add_wayland_protocol(MayaFluxLib
        PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/stable/presentation-time/presentation-time.xml"
        BASENAME "presentation-time"
    )
endif()

set_target_properties(MayaFluxLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_compile_definitions(MayaFluxLib PUBLIC
                           VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
                           ASIO_STANDALONE)

#==============================================================
#                   Target Includes
#==============================================================

if(APPLE)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Backends/System/NSBackend.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-include;${CMAKE_SOURCE_DIR}/cmake/pch.h"
    )
endif()

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

if(UNIX AND NOT APPLE)
    if(MAYAFLUX_AUDIO_BACKEND STREQUAL "PIPEWIRE_BACKEND")
        target_include_directories(MayaFluxLib PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
    endif()
    target_include_directories(MayaFluxLib PRIVATE
                               "${CMAKE_CURRENT_BINARY_DIR}/wayland_generated")

    target_include_directories(MayaFluxLib PRIVATE ${DBUS_INCLUDE_DIRS})
endif()

#==============================================================
#                   Target Links
#==============================================================

if(WIN32)
    target_link_libraries(MayaFluxLib PUBLIC Transitive_MD)
else()
    target_link_libraries(MayaFluxLib PUBLIC Transitive)
endif()

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
        Eigen3::Eigen
        glm::glm
        Vulkan::Vulkan
        FFmpeg::avcodec
        FFmpeg::avformat
        FFmpeg::avutil
        FFmpeg::swresample
        FFmpeg::swscale
        FFmpeg::avdevice
        hidapi::hidapi
        asio::asio
        assimp::assimp
        utf8proc::utf8proc
        nlohmann_json::nlohmann_json
        tinyexr
        winmm
        avrt
        ole32
        freetype
        gdi32
        user32
        shell32
        opengl32
    )
else()
    if(APPLE)
        target_link_libraries(MayaFluxLib PRIVATE oneDPL)
        target_link_libraries(MayaFluxLib PUBLIC fmt::fmt)
        target_link_libraries(MayaFluxLib PUBLIC Vulkan::Vulkan PkgConfig::Glfw)
        target_link_libraries(MayaFluxLib PRIVATE "-framework CoreFoundation")
        target_link_libraries(MayaFluxLib PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
        target_link_libraries(MayaFluxLib PRIVATE "-framework CoreMIDI")
        target_link_libraries(
            MayaFluxLib PRIVATE
            "-framework CoreAudio"
            "-framework AudioUnit"
            "-framework AudioToolbox"
            )

        message(STATUS "MayaFlux Journal: Using fmt library on macOS")
    else()
        target_link_libraries(MayaFluxLib PUBLIC
        PkgConfig::Vulkan
        PkgConfig::WAYLAND_CLIENT
        PkgConfig::XKBCOMMON
        PkgConfig::Fontconfig
        asound
        )

        target_link_libraries(MayaFluxLib PRIVATE ${PIPEWIRE_LIBRARIES} ${DBUS_LIBRARIES} )
        message(STATUS "PipeWire backend enabled (${PIPEWIRE_VERSION})")
    endif()

    target_link_libraries(MayaFluxLib PUBLIC
        TBB::tbb
        glm::glm
        PkgConfig::Eigen
        PkgConfig::HIDAPI
        PkgConfig::Asio
        PkgConfig::Assimp
        PkgConfig::LIBAVCODEC
        PkgConfig::LIBAVFORMAT
        PkgConfig::LIBAVUTIL
        PkgConfig::LIBSWRESAMPLE
        PkgConfig::LIBSWSCALE
        PkgConfig::LIBAVDEVICE
        Freetype::Freetype
        PkgConfig::UTF8PROC
        PkgConfig::nlohmann_json
        tinyexr
    )
endif()
