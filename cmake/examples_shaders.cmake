set(EXAMPLES_SHADER_DIR "${CMAKE_SOURCE_DIR}/src/examples/shaders")

if(NOT EXISTS "${EXAMPLES_SHADER_DIR}")
    return()
endif()

file(GLOB_RECURSE EXAMPLE_SHADER_FILES
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

foreach(SHADER_FILE ${EXAMPLE_SHADER_FILES})
    set(OUTPUT_FILE "${SHADER_FILE}.spv")

    add_custom_command(
        OUTPUT  "${OUTPUT_FILE}"
        COMMAND glslc "${SHADER_FILE}" -o "${OUTPUT_FILE}"
        DEPENDS "${SHADER_FILE}"
        COMMENT "Compiling example shader: ${SHADER_FILE}"
        VERBATIM
    )

    list(APPEND EXAMPLE_SHADER_OUTPUTS "${OUTPUT_FILE}")
endforeach()

add_custom_target(compile_example_shaders
    DEPENDS ${EXAMPLE_SHADER_OUTPUTS}
)

message(STATUS "Example shaders: ${EXAMPLES_SHADER_DIR} (${CMAKE_MATCH_COUNT} files, MAYAFLUX_DEV only)")
