set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        message(STATUS "ccache found: ${CCACHE_PROGRAM}")
    else()
        message(STATUS "ccache not found, building without cache")
    endif()

    add_compile_options(-pipe)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(-mavx -mavx2)
        message (STATUS "AVX and AVX2 support enabled for x86_64 architecture")
    endif()

elseif(MSVC)
    add_compile_options(
        /W4
        /MP
    )
endif()
