# add_definitions(${CLANG_DEFINITIONS})

set(LILA_SOURCES
    Lila.cpp
    ClangInterpreter.cpp
    EventBus.cpp
    Server.cpp
    WindowsJITSymbols.cpp
)

set(LILA_HEADERS
    ClangInterpreter.hpp
    Commentator.hpp
    EventBus.hpp
    LiveAid.hpp
    Lila.hpp
    Server.hpp
    ServerThread.hpp
)

add_library(Lila SHARED ${LILA_SOURCES} ${LILA_HEADERS})
target_sources(Lila PRIVATE ${CONFIG_IMPL})

if(WIN32)
    target_compile_definitions(Lila PRIVATE LILA_EXPORTS)
    set_target_properties(Lila PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

target_include_directories(Lila PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

target_precompile_headers(Lila PUBLIC ../../cmake/pch.h)
target_include_directories(Lila PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(CLANG_DEFINITIONS)
    target_compile_definitions(Lila PRIVATE ${CLANG_DEFINITIONS})
endif()

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(Lila PUBLIC ${LLVM_DEFINITIONS_LIST})

set_target_properties(Lila PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(UNIX)
    execute_process(
        COMMAND llvm-config --ldflags
        OUTPUT_VARIABLE LLVM_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND llvm-config --libs core orcjit native irreader support
        OUTPUT_VARIABLE LLVM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND llvm-config --system-libs
        OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    target_link_libraries(Lila PUBLIC
        ${LLVM_LDFLAGS}
        ${LLVM_LIBS}
        ${LLVM_SYSTEM_LIBS}
    )
    target_link_libraries(Lila PRIVATE clang-cpp)
else()
    target_link_libraries(Lila PRIVATE
        LLVMSupport
        LLVMCore
        LLVMOrcJIT
        LLVMIRReader
        LLVMExecutionEngine
        LLVMRuntimeDyld
        LLVMTarget
        LLVMX86CodeGen
        LLVMMC
        LLVMBitReader
        LLVMAsmParser
        LLVMTransformUtils
        clangAST
        clangBasic
        clangCodeGen
        clangDriver
        clangFrontend
        clangLex
        clangParse
        clangSema
        clangSerialization
        clangInterpreter
        ws2_32
        legacy_stdio_definitions
    )
    target_link_options(Lila PRIVATE
        /DEFAULTLIB:vcruntime.lib
        /DEFAULTLIB:msvcrt.lib
    )
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(Lila PUBLIC
        -Wl,--no-as-needed
        MayaFluxLib
        -Wl,--as-needed
    )
else()
    target_link_libraries(Lila PUBLIC MayaFluxLib)
endif()
