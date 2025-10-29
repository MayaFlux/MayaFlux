# =============================================================================
# Shader Dependencies (SPIRV-Reflect and Shaderc)
# =============================================================================

option(MAYAFLUX_USE_SPIRV_REFLECT "Enable shader reflection via SPIRV-Reflect" ON)
option(MAYAFLUX_USE_SHADERC "Enable GLSL compilation via Shaderc" ON)

if(MAYAFLUX_USE_SPIRV_REFLECT)
    message(STATUS "SPIRV-Reflect: Building in isolated subdirectory...")

    add_subdirectory(${CMAKE_SOURCE_DIR}/cmake/spirv_reflect
                        ${CMAKE_BINARY_DIR}/spirv_reflect_build
                        EXCLUDE_FROM_ALL)

    message(STATUS "SPIRV-Reflect fetched successfully")
    message(STATUS "Shader reflection: ENABLED")
else()
    message(STATUS "Shader reflection: DISABLED (manual descriptor layouts required)")
endif()

if(MAYAFLUX_USE_SHADERC)
    if(UNIX)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(shaderc REQUIRED IMPORTED_TARGET shaderc)

        if(shaderc_FOUND)
            message(STATUS "Shaderc: Using system package")
        else()
            message(WARNING "Shaderc not found. Please install via system package manager:")
            message(WARNING "  Ubuntu/Debian: sudo apt install libshaderc-dev")
            message(WARNING "  Fedora: sudo dnf install shaderc-devel")
            message(WARNING "  MacOS: Run the provided scripts/setup_macos.sh")
            message(WARNING "  Arch: sudo pacman -S shaderc")
        endif()
    elseif(WIN32)
        find_package(Vulkan REQUIRED)
        if(TARGET Vulkan::shaderc_combined)
            message(STATUS "Shaderc: Using Vulkan SDK shaderc_combined")
        else()
            find_library(SHADERC_COMBINED_LIB
                NAMES shaderc_combined
                PATHS ${Vulkan_LIBRARY_DIR}
                NO_DEFAULT_PATH
            )
            find_path(SHADERC_INCLUDE_DIR
                NAMES shaderc/shaderc.h
                PATHS ${Vulkan_INCLUDE_DIR}
                NO_DEFAULT_PATH
            )
            if(SHADERC_COMBINED_LIB AND SHADERC_INCLUDE_DIR)
                add_library(shaderc_combined STATIC IMPORTED)
                set_target_properties(shaderc_combined PROPERTIES
                    IMPORTED_LOCATION ${SHADERC_COMBINED_LIB}
                    INTERFACE_INCLUDE_DIRECTORIES ${SHADERC_INCLUDE_DIR}
                )
                message(STATUS "Shaderc: Found in Vulkan SDK (manual)")
            else()
                message(WARNING "Shaderc not found in Vulkan SDK. Runtime GLSL compilation disabled.")
                set(MAYAFLUX_USE_SHADERC OFF)
            endif()
        endif()
    endif()
    message(STATUS "GLSL compilation: ENABLED")
else()
    message(STATUS "GLSL compilation: DISABLED (pre-compiled SPIR-V required)")
endif()

set(SHADERS_DIR "${DATA_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})


file(GLOB_RECURSE SHADER_FILES
    "${SHADERS_DIR}/*.vert"
    "${SHADERS_DIR}/*.frag"
    "${SHADERS_DIR}/*.comp"
    "${SHADERS_DIR}/*.geom"
    "${SHADERS_DIR}/*.tesc"
    "${SHADERS_DIR}/*.tese"
)

if(SHADER_FILES)
    set(SHADER_OUTPUTS)

    foreach(SHADER_FILE ${SHADER_FILES})
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
        set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

        if(NOT OUTPUT_FILE IN_LIST SHADER_OUTPUTS)
            add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND glslc ${SHADER_FILE} -o ${OUTPUT_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling shader: ${SHADER_NAME}"
            VERBATIM
        )
            list(APPEND SHADER_OUTPUTS ${OUTPUT_FILE})
        endif()
    endforeach()

    if(SHADER_OUTPUTS)
        add_custom_target(compile_shaders ALL DEPENDS ${SHADER_OUTPUTS})
        message(STATUS "Found ${CMAKE_MATCH_COUNT} shaders to compile")
    else()
        message(STATUS "No shaders found in ${SHADERS_DIR}")
    endif()
endif()
