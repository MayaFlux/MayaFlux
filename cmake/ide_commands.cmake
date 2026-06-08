if(WIN32)
    message(STATUS "=== Visual Studio Setup ===")

    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

    foreach(_target
        acc_gen
        clang-tablegen-targets
        intrinsics_gen
        omp_gen
        target_parser_gen
        vt_gen
    )
        if(TARGET ${_target})
            set_target_properties(${_target} PROPERTIES FOLDER "LLVM")
        endif()
    endforeach()

    if(TARGET miniz)
        set_target_properties(miniz PROPERTIES FOLDER "DEPS")
    endif()

    add_custom_target(regenerate_solution
        COMMAND powershell -ExecutionPolicy Bypass -File ${CMAKE_SOURCE_DIR}/scripts/win64/setup_visual_studio.ps1
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Regenerating Visual Studio solution"
        VERBATIM
    )
endif()


if(UNIX)
    execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json)
endif()
