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

    if(TARGET tinyvdb)
        set_target_properties(tinyvdb PROPERTIES FOLDER "DEPS")
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
elseif(WIN32 AND CMAKE_GENERATOR MATCHES "^Ninja" AND CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/compile_commands.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
        DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
        VERBATIM
    )
    add_custom_target(copy_compile_commands ALL
        DEPENDS "${CMAKE_SOURCE_DIR}/compile_commands.json"
    )
endif()
