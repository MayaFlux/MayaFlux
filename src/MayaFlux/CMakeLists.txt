set(MAYAFLUX_ALL_SOURCES "")
set(MAYAFLUX_ALL_HEADERS "")
set(MAYAFLUX_HEAVY_SOURCES "")

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

list(REMOVE_ITEM MAYAFLUX_ALL_SOURCES ${MAYAFLUX_HEAVY_SOURCES})

add_library(MayaFluxHeavy OBJECT ${MAYAFLUX_HEAVY_SOURCES})
add_library(MayaFluxLib SHARED ${MAYAFLUX_ALL_SOURCES} ${MAYAFLUX_ALL_HEADERS})

set_target_properties(MayaFluxHeavy PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

#==============================================================
#        Requirements shared by both parts of the library
#==============================================================

function(mayaflux_library_requirements target)
    if(WIN32)
        add_compile_definitions(USE_PPL_BACKEND)
        target_compile_definitions(${target}
            PRIVATE MAYAFLUX_EXPORTS VULKAN_HPP_STORAGE_SHARED_EXPORT
            PUBLIC VULKAN_HPP_STORAGE_SHARED
        )
        if(MSVC)
            target_compile_options(${target} PRIVATE /MD)
        endif()
    endif()

    target_compile_definitions(${target} PUBLIC
                               VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
                               ASIO_STANDALONE)

    target_include_directories(${target} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # Points to src/ for #include "MayaFlux/..."
    )
    if(TARGET compile_shaders)
        target_include_directories(${target} PRIVATE ${SHADER_OUTPUT_DIR})
    endif()
    target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/include)

    if(WIN32)
        target_include_directories(${target} PRIVATE ${Vulkan_INCLUDE_DIRS})
    endif()
    if(APPLE)
        target_include_directories(${target} PRIVATE ${STB_INCLUDE_DIR})
    endif()

    if(UNIX AND NOT APPLE)
        if(MAYAFLUX_AUDIO_BACKEND STREQUAL "PIPEWIRE_BACKEND")
            target_include_directories(${target} PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
        endif()
        target_include_directories(${target} PRIVATE
                                   "${CMAKE_CURRENT_BINARY_DIR}/wayland_generated")

        target_include_directories(${target} PRIVATE ${DBUS_INCLUDE_DIRS})
    endif()

    target_link_libraries(${target} PUBLIC Transitive Transitive_Live)

    if(WIN32)
        if(TARGET Vulkan::shaderc_combined)
            target_link_libraries(${target} PRIVATE Vulkan::shaderc_combined)
        else()
            target_link_libraries(${target} PRIVATE shaderc_combined)
        endif()
    else()
        target_link_libraries(${target} PRIVATE PkgConfig::shaderc)
        target_link_libraries(${target} PRIVATE ${SPIRV_TOOLS_TARGET})
    endif()

    target_link_libraries(${target} PRIVATE ${SPIRV_CROSS_LIBRARIES})

    if(WIN32)
        target_link_libraries(${target} PUBLIC
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
            Alembic::Alembic
            utf8proc::utf8proc
            pugixml::pugixml
            tinyexr
            tinyvdb
            dlpack
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
            target_compile_options(${target} PUBLIC -fexperimental-library)
            target_link_options(${target} PUBLIC -fexperimental-library)
            target_link_libraries(${target} PUBLIC Vulkan::Vulkan)
            target_link_libraries(${target} PRIVATE "-framework CoreFoundation")
            target_link_libraries(${target} PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers" "-framework QuartzCore")
            target_link_libraries(${target} PRIVATE "-framework CoreMIDI")
            target_link_libraries(${target} PRIVATE "-framework AVFoundation")
            target_link_libraries(
                ${target} PRIVATE
                "-framework CoreAudio"
                "-framework AudioUnit"
                "-framework AudioToolbox"
                )
        else()
            target_link_libraries(${target} PUBLIC
            PkgConfig::Vulkan
            PkgConfig::WAYLAND_CLIENT
            PkgConfig::XKBCOMMON
            PkgConfig::Fontconfig
            TBB::tbb
            asound
            )

            target_link_libraries(${target} PRIVATE ${PIPEWIRE_LIBRARIES} ${DBUS_LIBRARIES} )
            message(STATUS "PipeWire backend enabled (${PIPEWIRE_VERSION})")
        endif()

        target_link_libraries(${target} PUBLIC
            glm::glm
            PkgConfig::Eigen
            PkgConfig::HIDAPI
            PkgConfig::Asio
            PkgConfig::Assimp
            Alembic::Alembic
            PkgConfig::LIBAVCODEC
            PkgConfig::LIBAVFORMAT
            PkgConfig::LIBAVUTIL
            PkgConfig::LIBSWRESAMPLE
            PkgConfig::LIBSWSCALE
            PkgConfig::LIBAVDEVICE
            Freetype::Freetype
            PkgConfig::UTF8PROC
            PkgConfig::PUGIXML
            tinyexr
            tinyvdb
            dlpack
        )
    endif()
endfunction()

mayaflux_library_requirements(MayaFluxHeavy)
mayaflux_library_requirements(MayaFluxLib)

#==============================================================
#               Target Definitions and Properties
#==============================================================

if(WIN32 AND MSVC)
    target_link_options(MayaFluxLib PRIVATE
        "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/IGNORE:4099>")
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

    mf_add_wayland_protocol(MayaFluxLib
        PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
        BASENAME "pointer-constraints-unstable-v1"
    )

    mf_add_wayland_protocol(MayaFluxLib
        PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
        BASENAME "relative-pointer-unstable-v1"
    )

    mf_add_wayland_protocol(MayaFluxLib
        PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/unstable/tablet/tablet-unstable-v2.xml"
        BASENAME "tablet-unstable-v2"
    )

    mf_add_wayland_protocol(MayaFluxLib
        PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/staging/cursor-shape/cursor-shape-v1.xml"
        BASENAME "cursor-shape-v1"
    )
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    get_target_property(MAYAFLUX_WAYLAND_PROTOCOLS MayaFluxLib LINK_LIBRARIES)
    list(FILTER MAYAFLUX_WAYLAND_PROTOCOLS INCLUDE REGEX "^wl_proto_")
    add_dependencies(MayaFluxHeavy ${MAYAFLUX_WAYLAND_PROTOCOLS})
endif()

set_target_properties(MayaFluxLib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(APPLE)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Backends/System/NSBackend.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-include;${CMAKE_SOURCE_DIR}/cmake/pch.h"
    )
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Backends/Windowing/Cocoa/CocoaWindow.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-include;${CMAKE_SOURCE_DIR}/cmake/pch.h;-fobjc-arc"
    )
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/IO/Camera/FFmpegCameraDiscoveryMac.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-include;${CMAKE_SOURCE_DIR}/cmake/pch.h;-fobjc-arc"
    )
endif()

#==============================================================
#                   Precompiled Headers
#==============================================================

target_precompile_headers(MayaFluxLib PUBLIC ${CMAKE_SOURCE_DIR}/cmake/pch.h)

target_precompile_headers(MayaFluxHeavy PRIVATE
    ${CMAKE_SOURCE_DIR}/cmake/pch.h
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:<vulkan/vulkan.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:<glm/glm.hpp$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:<Eigen/Dense$<ANGLE-R>>"
)

target_link_libraries(MayaFluxLib PRIVATE MayaFluxHeavy)
