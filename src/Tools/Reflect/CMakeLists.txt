add_executable(mayaflux_reflect
    MayaFluxReflect.cpp
    Collector.cpp
    Discovery.cpp
    Scan.cpp
    Sidecar.cpp
)

set(MAYAFLUX_REFLECT_RESOURCE_DIR "")
foreach(_llvm_dir IN LISTS LLVM_LIBRARY_DIRS ITEMS "${LLVM_INSTALL_PREFIX}/lib")
    if(NOT MAYAFLUX_REFLECT_RESOURCE_DIR AND IS_DIRECTORY "${_llvm_dir}/clang/${LLVM_VERSION_MAJOR}")
        set(MAYAFLUX_REFLECT_RESOURCE_DIR "${_llvm_dir}/clang/${LLVM_VERSION_MAJOR}")
    endif()
endforeach()

if(MAYAFLUX_REFLECT_RESOURCE_DIR)
    target_compile_definitions(mayaflux_reflect PRIVATE
        MAYAFLUX_REFLECT_RESOURCE_DIR="${MAYAFLUX_REFLECT_RESOURCE_DIR}")
endif()

target_include_directories(mayaflux_reflect PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_precompile_headers(mayaflux_reflect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pch.h)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(mayaflux_reflect PRIVATE ${LLVM_DEFINITIONS_LIST})

if(CLANG_DEFINITIONS)
    target_compile_definitions(mayaflux_reflect PRIVATE ${CLANG_DEFINITIONS})
endif()

set_target_properties(mayaflux_reflect PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

if(NOT MSVC AND NOT LLVM_ENABLE_RTTI)
    target_compile_options(mayaflux_reflect PRIVATE -fno-rtti)
endif()

if(UNIX)
    execute_process(
        COMMAND llvm-config --ldflags
        OUTPUT_VARIABLE LLVM_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND llvm-config --libs support
        OUTPUT_VARIABLE LLVM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND llvm-config --system-libs
        OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    target_link_libraries(mayaflux_reflect PRIVATE
        ${LLVM_LDFLAGS}
        ${LLVM_LIBS}
        ${LLVM_SYSTEM_LIBS}
        clang-cpp
    )
else()
    target_link_libraries(mayaflux_reflect PRIVATE
        clangTooling
        clangToolingCore
        clangFrontend
        clangDriver
        clangSerialization
        clangParse
        clangSema
        clangAnalysis
        clangASTMatchers
        clangAST
        clangEdit
        clangLex
        clangBasic
        LLVMSupport
        LLVMOption
        LLVMFrontendOpenMP
        legacy_stdio_definitions
    )
    target_link_options(mayaflux_reflect PRIVATE
        /DEFAULTLIB:vcruntime.lib
        /DEFAULTLIB:msvcrt.lib
    )
endif()
