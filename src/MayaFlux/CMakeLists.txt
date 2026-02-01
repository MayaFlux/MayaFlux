file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(MayaFluxLib SHARED ${SOURCES} ${HEADERS})
target_sources(MayaFluxLib PRIVATE ${CONFIG_IMPL})

if(WIN32)
    target_compile_definitions(MayaFluxLib PRIVATE MAYAFLUX_EXPORTS)
    if(MSVC)
        target_compile_options(MayaFluxLib PRIVATE /MD)

        if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            target_compile_options(MayaFluxLib PRIVATE "/Zc:externConstexpr-")
            target_link_options(MayaFluxLib PRIVATE "/IGNORE:4099")
        endif()
    endif()
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


if(TARGET compile_shaders)
    target_include_directories(MayaFluxLib PRIVATE ${SHADER_OUTPUT_DIR})
endif()

if(WIN32)
    target_include_directories(MayaFluxLib PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()
target_link_libraries(MayaFluxLib PRIVATE ${SPIRV_CROSS_LIBRARIES})
target_compile_definitions(MayaFluxLib PRIVATE MAYAFLUX_USE_SPIRV_CROSS)
message(STATUS "Core: Linked with SPIRV-Cross")


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

if(MAYAFLUX_ENABLE_HID)
    target_compile_definitions(MayaFluxLib PRIVATE MAYAFLUX_HID_BACKEND)

    if(WIN32)
        target_link_libraries(MayaFluxLib PRIVATE hidapi::hidapi)
    else()
        target_link_libraries(MayaFluxLib PRIVATE PkgConfig::HIDAPI)
    endif()

    message(STATUS "Core: HID input backend enabled (HIDAPI ${HIDAPI_VERSION})")
endif()

target_include_directories(MayaFluxLib PRIVATE ${CMAKE_BINARY_DIR}/include)

if(APPLE)
    target_include_directories(MayaFluxLib PRIVATE ${STB_INCLUDE_DIR})
endif()

if(WIN32)
    add_compile_definitions(USE_PPL_BACKEND)

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
        target_link_libraries(MayaFluxLib PRIVATE "-framework CoreFoundation")
    else()
        add_compile_definitions(USE_TBB_BACKEND)
        target_link_libraries(MayaFluxLib PUBLIC PkgConfig::Vulkan)
    endif()

    target_link_libraries(MayaFluxLib PUBLIC TBB::tbb glm::glm)

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
