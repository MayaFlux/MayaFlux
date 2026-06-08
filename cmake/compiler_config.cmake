set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(WIN32)
    execute_process(
        COMMAND cmd /c set
        OUTPUT_VARIABLE GLOBAL_ENVIRONMENT_BLOCK
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(REGEX REPLACE "\r?\n" ";" ENVIRONMENT_LINES "${GLOBAL_ENVIRONMENT_BLOCK}")

    foreach(ENV_LINE IN LISTS ENVIRONMENT_LINES)
        string(FIND "${ENV_LINE}" "=" EQUAL_SIGN_INDEX)
        
        if(EQUAL_SIGN_INDEX GREATER 0)
            string(SUBSTRING "${ENV_LINE}" 0 ${EQUAL_SIGN_INDEX} ENV_KEY)
            math(EXPR VALUE_START_INDEX "${EQUAL_SIGN_INDEX} + 1")
            string(SUBSTRING "${ENV_LINE}" ${VALUE_START_INDEX} -1 ENV_VALUE)

            if(ENV_VALUE MATCHES "\\\\")
                file(TO_CMAKE_PATH "${ENV_VALUE}" SANITIZED_VALUE)
                set(ENV{${ENV_KEY}} "${SANITIZED_VALUE}")
            endif()
        endif()
    endforeach()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        message(STATUS "ccache found: ${CCACHE_PROGRAM}")
    else()
        message(STATUS "ccache not found, building without cache")
    endif()

    add_compile_options(-pipe)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=always)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fcolor-diagnostics)
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(-mavx -mavx2)
        message (STATUS "AVX and AVX2 support enabled for x86_64 architecture")
    endif()

elseif(MSVC)
    add_compile_options( /MP)
endif()
