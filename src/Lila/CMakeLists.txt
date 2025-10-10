if(NOT LLVM_FOUND)
    message(WARNING "LLVM not found - Lila live coding library will not be built")
    return()
endif()

set(LILA_SOURCES
    Lila.cpp
    JITCompiler.cpp
)

set(LILA_HEADERS
    JITCompiler.hpp
    SimpleSymbolParser.hpp
    SymbolRegistry.hpp
    Lila.hpp
    pch.h
    config.h
)

add_library(Lila SHARED ${LILA_SOURCES} ${LILA_HEADERS})


target_include_directories(Lila PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For #include "Lila/..."
    ${LLVM_INCLUDE_DIRS}
)

target_precompile_headers(Lila PUBLIC ../../cmake/pch.h)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(Lila PUBLIC ${LLVM_DEFINITIONS_LIST})

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

target_link_libraries(Lila PUBLIC MayaFluxLib)

set_target_properties(Lila PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS Lila
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${LILA_HEADERS}
    DESTINATION include/Lila
)
