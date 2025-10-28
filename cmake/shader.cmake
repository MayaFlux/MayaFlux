# =============================================================================
# Shader Dependencies (SPIRV-Reflect and Shaderc)
# =============================================================================

option(MAYAFLUX_USE_SPIRV_REFLECT "Enable shader reflection via SPIRV-Reflect" ON)
option(MAYAFLUX_USE_SHADERC "Enable GLSL compilation via Shaderc" ON)

if(MAYAFLUX_USE_SPIRV_REFLECT)
    message(STATUS "SPIRV-Reflect: Fetching from GitHub...")

    include(FetchContent)
    FetchContent_Declare(
        spirv_reflect
        URL https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/heads/main.zip
    )

    FetchContent_GetProperties(spirv_reflect)
    if(NOT spirv_reflect_POPULATED)
        FetchContent_MakeAvailable(spirv_reflect)
    endif()

    add_library(mayaflux-spirv-reflect STATIC
        ${spirv_reflect_SOURCE_DIR}/spirv_reflect.c
    )

    target_include_directories(mayaflux-spirv-reflect
        PUBLIC ${spirv_reflect_SOURCE_DIR}
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(mayaflux-spirv-reflect PRIVATE -w)
    else()
        target_compile_definitions(mayaflux-spirv-reflect
            PUBLIC SPIRV_REFLECT_IMPLEMENTATION)
    endif()

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
            message(WARNING "  Arch: sudo pacman -S shaderc")
        endif()
    endif()
    message(STATUS "GLSL compilation: ENABLED")
else()
    message(STATUS "GLSL compilation: DISABLED (pre-compiled SPIR-V required)")
endif()

set(DATA_DIR "${CMAKE_SOURCE_DIR}/data")
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
