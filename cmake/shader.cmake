# =============================================================================
# Shader Dependencies (SPIRV-Cross, Shaderc, SPIRV-Tools)
# =============================================================================

if(WIN32)
    cmake_path(CONVERT "$ENV{VULKAN_SDK}" TO_CMAKE_PATH_LIST _vulkan_sdk NORMALIZE)
    find_package(Vulkan REQUIRED)

    # SPIRV-Cross
    find_library(SPIRV_CROSS_CORE_LIB NAMES spirv-cross-core
        PATHS "${_vulkan_sdk}/Lib" REQUIRED NO_DEFAULT_PATH)
    find_library(SPIRV_CROSS_CPP_LIB NAMES spirv-cross-cpp
        PATHS "${_vulkan_sdk}/Lib" REQUIRED NO_DEFAULT_PATH)
    find_library(SPIRV_CROSS_GLSL_LIB NAMES spirv-cross-glsl
        PATHS "${_vulkan_sdk}/Lib" REQUIRED NO_DEFAULT_PATH)
    find_path(SPIRV_CROSS_INCLUDE_DIR spirv_cross/spirv_cross.hpp
        PATHS "${_vulkan_sdk}/Include" REQUIRED NO_DEFAULT_PATH)

    add_library(spirv-cross-core STATIC IMPORTED)
    set_target_properties(spirv-cross-core PROPERTIES
        IMPORTED_LOCATION_DEBUG          ${SPIRV_CROSS_CORE_LIB}
        IMPORTED_LOCATION_RELEASE        ${SPIRV_CROSS_CORE_LIB}
        IMPORTED_LOCATION_RELWITHDEBINFO ${SPIRV_CROSS_CORE_LIB}
        IMPORTED_LOCATION_MINSIZEREL     ${SPIRV_CROSS_CORE_LIB}
        INTERFACE_INCLUDE_DIRECTORIES    ${SPIRV_CROSS_INCLUDE_DIR}
    )

    add_library(spirv-cross-glsl STATIC IMPORTED)
    set_target_properties(spirv-cross-glsl PROPERTIES
        IMPORTED_LOCATION_DEBUG          ${SPIRV_CROSS_GLSL_LIB}
        IMPORTED_LOCATION_RELEASE        ${SPIRV_CROSS_GLSL_LIB}
        IMPORTED_LOCATION_RELWITHDEBINFO ${SPIRV_CROSS_GLSL_LIB}
        IMPORTED_LOCATION_MINSIZEREL     ${SPIRV_CROSS_GLSL_LIB}
        INTERFACE_INCLUDE_DIRECTORIES    ${SPIRV_CROSS_INCLUDE_DIR}
    )

    add_library(spirv-cross-cpp STATIC IMPORTED)
    set_target_properties(spirv-cross-cpp PROPERTIES
        IMPORTED_LOCATION_DEBUG          ${SPIRV_CROSS_CPP_LIB}
        IMPORTED_LOCATION_RELEASE        ${SPIRV_CROSS_CPP_LIB}
        IMPORTED_LOCATION_RELWITHDEBINFO ${SPIRV_CROSS_CPP_LIB}
        IMPORTED_LOCATION_MINSIZEREL     ${SPIRV_CROSS_CPP_LIB}
        INTERFACE_INCLUDE_DIRECTORIES    ${SPIRV_CROSS_INCLUDE_DIR}
        INTERFACE_LINK_LIBRARIES         spirv-cross-glsl
    )

    set(SPIRV_CROSS_LIBRARIES
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-cpp
    )

    message(STATUS "SPIRV-Cross: Found in Vulkan SDK (Windows)")
    message(WARNING "SPIRV-Cross: Using Release libraries for all configurations (Vulkan SDK has no Debug builds)")

    # SPIRV-Tools
    find_library(SPIRV_TOOLS_IMPLIB NAMES SPIRV-Tools-shared
        PATHS "${_vulkan_sdk}/Lib" NO_DEFAULT_PATH REQUIRED)
    find_program(SPIRV_TOOLS_DLL NAMES SPIRV-Tools-shared.dll
        PATHS "${_vulkan_sdk}/Bin" NO_DEFAULT_PATH REQUIRED)
    find_path(SPIRV_TOOLS_INCLUDE_DIR spirv-tools/libspirv.hpp
        PATHS "${_vulkan_sdk}/Include" NO_DEFAULT_PATH REQUIRED)

    add_library(SPIRV-Tools-shared SHARED IMPORTED)
    set_target_properties(SPIRV-Tools-shared PROPERTIES
        IMPORTED_LOCATION                "${SPIRV_TOOLS_DLL}"
        IMPORTED_IMPLIB                  "${SPIRV_TOOLS_IMPLIB}"
        IMPORTED_LOCATION_DEBUG          "${SPIRV_TOOLS_DLL}"
        IMPORTED_IMPLIB_DEBUG            "${SPIRV_TOOLS_IMPLIB}"
        IMPORTED_LOCATION_RELEASE        "${SPIRV_TOOLS_DLL}"
        IMPORTED_IMPLIB_RELEASE          "${SPIRV_TOOLS_IMPLIB}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${SPIRV_TOOLS_DLL}"
        IMPORTED_IMPLIB_RELWITHDEBINFO   "${SPIRV_TOOLS_IMPLIB}"
        IMPORTED_LOCATION_MINSIZEREL     "${SPIRV_TOOLS_DLL}"
        IMPORTED_IMPLIB_MINSIZEREL       "${SPIRV_TOOLS_IMPLIB}"
        INTERFACE_INCLUDE_DIRECTORIES    "${SPIRV_TOOLS_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS    "SPIRV_TOOLS_SHARED_LIB"
    )
    message(STATUS "SPIRV-Tools: Found in Vulkan SDK (Windows)")

    # Shaderc
    if(TARGET Vulkan::shaderc_combined)
        message(STATUS "Shaderc: Using Vulkan SDK shaderc_combined")
    else()
        find_library(SHADERC_COMBINED_LIB NAMES shaderc_combined
            PATHS "${_vulkan_sdk}/Lib" NO_DEFAULT_PATH REQUIRED)
        find_path(SHADERC_INCLUDE_DIR NAMES shaderc/shaderc.h
            PATHS "${_vulkan_sdk}/Include" NO_DEFAULT_PATH REQUIRED)
        add_library(shaderc_combined STATIC IMPORTED)
        set_target_properties(shaderc_combined PROPERTIES
            IMPORTED_LOCATION             ${SHADERC_COMBINED_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${SHADERC_INCLUDE_DIR}
        )
        message(STATUS "Shaderc: Found in Vulkan SDK (Windows)")
    endif()

else()
    find_package(PkgConfig REQUIRED)

    # SPIRV-Cross
    find_library(SPIRV_CROSS_CORE_LIB NAMES spirv-cross-core REQUIRED)
    find_library(SPIRV_CROSS_CPP_LIB NAMES spirv-cross-cpp REQUIRED)
    find_path(SPIRV_CROSS_INCLUDE_DIR spirv_cross/spirv_cross.hpp REQUIRED)

    add_library(spirv-cross-core SHARED IMPORTED)
    set_target_properties(spirv-cross-core PROPERTIES
        IMPORTED_LOCATION             ${SPIRV_CROSS_CORE_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${SPIRV_CROSS_INCLUDE_DIR}
    )

    add_library(spirv-cross-cpp SHARED IMPORTED)
    set_target_properties(spirv-cross-cpp PROPERTIES
        IMPORTED_LOCATION             ${SPIRV_CROSS_CPP_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${SPIRV_CROSS_INCLUDE_DIR}
    )

    set(SPIRV_CROSS_LIBRARIES
        spirv-cross-core
        spirv-cross-cpp
    )

    message(STATUS "SPIRV-Cross: Found C++ libraries at ${SPIRV_CROSS_CORE_LIB}")

    # SPIRV-Tools
    # Ubuntu/Debian: spirv-tools-dev + spirv-tools
    # Fedora:        spirv-tools-devel
    # Arch:          spirv-tools
    # macOS:         brew install spirv-tools
    find_package(SPIRV-Tools CONFIG QUIET)

    if(TARGET SPIRV-Tools-shared)
        message(STATUS "SPIRV-Tools: Found via CMake config (${SPIRV-Tools_VERSION})")
        set(SPIRV_TOOLS_TARGET SPIRV-Tools-shared)
    else()
        find_library(SPIRV_TOOLS_LIB_PATH NAMES SPIRV-Tools-shared SPIRV-Tools)
        find_path(SPIRV_TOOLS_INCLUDE_PATH spirv-tools/libspirv.hpp REQUIRED)
        if(SPIRV_TOOLS_LIB_PATH MATCHES "\\.a$")
            add_library(SPIRV-Tools STATIC IMPORTED)
            set_target_properties(SPIRV-Tools PROPERTIES
                IMPORTED_LOCATION             ${SPIRV_TOOLS_LIB_PATH}
                INTERFACE_INCLUDE_DIRECTORIES ${SPIRV_TOOLS_INCLUDE_PATH}
            )
            set(SPIRV_TOOLS_TARGET SPIRV-Tools)
            message(STATUS "SPIRV-Tools: Found static library at ${SPIRV_TOOLS_LIB_PATH}")
        else()
            add_library(SPIRV-Tools-shared SHARED IMPORTED)
            set_target_properties(SPIRV-Tools-shared PROPERTIES
                IMPORTED_LOCATION             ${SPIRV_TOOLS_LIB_PATH}
                INTERFACE_INCLUDE_DIRECTORIES ${SPIRV_TOOLS_INCLUDE_PATH}
            )
            set(SPIRV_TOOLS_TARGET SPIRV-Tools-shared)
            message(STATUS "SPIRV-Tools: Found shared library at ${SPIRV_TOOLS_LIB_PATH}")
        endif()
    endif()

    # Shaderc
    # Ubuntu/Debian: libshaderc-dev
    # Fedora:        shaderc-devel
    # Arch:          shaderc
    # macOS:         scripts/setup_macos.sh
    pkg_check_modules(shaderc REQUIRED IMPORTED_TARGET shaderc)
    message(STATUS "Shaderc: Using system package")

endif()

message(STATUS "Shader reflection: ENABLED (SPIRV-Cross)")
message(STATUS "GLSL compilation:  ENABLED (Shaderc)")
message(STATUS "SPIR-V assembly:   ENABLED (SPIRV-Tools)")

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
                OUTPUT  ${OUTPUT_FILE}
                COMMAND glslc --target-env=vulkan1.3 ${SHADER_FILE} -o ${OUTPUT_FILE}
                DEPENDS ${SHADER_FILE}
                COMMENT "Compiling shader: ${SHADER_NAME}"
                VERBATIM
            )
            list(APPEND SHADER_OUTPUTS ${OUTPUT_FILE})
        endif()
    endforeach()

    if(SHADER_OUTPUTS)
        add_custom_target(compile_shaders ALL
            DEPENDS ${SHADER_OUTPUTS}
            SOURCES ${SHADER_FILES}
        )
        message(STATUS "Found ${CMAKE_MATCH_COUNT} shaders to compile")
    else()
        message(STATUS "No shaders found in ${SHADERS_DIR}")
    endif()
endif()
