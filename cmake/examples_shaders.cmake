set(EXAMPLES_SHADER_DIR "${CMAKE_SOURCE_DIR}/src/examples/shaders")

if(NOT EXISTS "${EXAMPLES_SHADER_DIR}")
    return()
endif()

file(GLOB_RECURSE EXAMPLE_SHADER_FILES CONFIGURE_DEPENDS
    "${EXAMPLES_SHADER_DIR}/*.vert"
    "${EXAMPLES_SHADER_DIR}/*.frag"
    "${EXAMPLES_SHADER_DIR}/*.comp"
    "${EXAMPLES_SHADER_DIR}/*.geom"
    "${EXAMPLES_SHADER_DIR}/*.tesc"
    "${EXAMPLES_SHADER_DIR}/*.tese"
    "${EXAMPLES_SHADER_DIR}/*.mesh"
    "${EXAMPLES_SHADER_DIR}/*.task"
    "${EXAMPLES_SHADER_DIR}/*.rgen"
    "${EXAMPLES_SHADER_DIR}/*.rchit"
    "${EXAMPLES_SHADER_DIR}/*.rmiss"
)

if(NOT EXAMPLE_SHADER_FILES)
    message(STATUS "No example shaders found in ${EXAMPLES_SHADER_DIR}")
    return()
endif()

set(EXAMPLE_SHADER_OUTPUTS)
set(EXAMPLE_SHADER_DEPS_DIR "${CMAKE_BINARY_DIR}/example_shader_deps")
file(MAKE_DIRECTORY "${EXAMPLE_SHADER_DEPS_DIR}")

foreach(SHADER_FILE ${EXAMPLE_SHADER_FILES})
    set(OUTPUT_FILE "${SHADER_FILE}.spv")
    string(MD5 SHADER_KEY "${SHADER_FILE}")
    set(DEPS_FILE "${EXAMPLE_SHADER_DEPS_DIR}/${SHADER_KEY}")

    add_custom_command(
        OUTPUT  "${OUTPUT_FILE}"
        COMMAND glslc --target-env=vulkan1.4 -MD -MF "${DEPS_FILE}.d" "${SHADER_FILE}" -o "${OUTPUT_FILE}"
        DEPENDS "${SHADER_FILE}"
        DEPFILE "${DEPS_FILE}.d"
        COMMENT "Compiling example shader: ${SHADER_FILE}"
        VERBATIM
    )

    list(APPEND EXAMPLE_SHADER_OUTPUTS "${OUTPUT_FILE}")

    if(SHADER_FILE MATCHES "_lit\\.frag$")
        string(REGEX REPLACE "_lit\\.frag$" "_lit_list.frag.spv" LIST_OUTPUT "${SHADER_FILE}")
        add_custom_command(
            OUTPUT  "${LIST_OUTPUT}"
            COMMAND glslc --target-env=vulkan1.4 -DMF_INFLUENCE_LIST -MD -MF "${DEPS_FILE}_list.d" "${SHADER_FILE}" -o "${LIST_OUTPUT}"
            DEPENDS "${SHADER_FILE}"
            DEPFILE "${DEPS_FILE}_list.d"
            COMMENT "Compiling example shader: ${LIST_OUTPUT}"
            VERBATIM
        )
        list(APPEND EXAMPLE_SHADER_OUTPUTS "${LIST_OUTPUT}")
    endif()
endforeach()

add_custom_target(compile_example_shaders
    DEPENDS ${EXAMPLE_SHADER_OUTPUTS}
)

message(STATUS "Example shaders: ${EXAMPLES_SHADER_DIR} (${CMAKE_MATCH_COUNT} files, MAYAFLUX_DEV only)")
