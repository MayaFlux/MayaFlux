if(NOT LLVM_FOUND)
    message(WARNING "LLVM not found - Lila live coding library will not be built")
    return()
endif()

find_package(Clang REQUIRED CONFIG)

include_directories(${CLANG_INCLUDE_DIRS})
add_definitions(${CLANG_DEFINITIONS})

set(LILA_SOURCES
    Lila.cpp
    ClangInterpreter.cpp
    EventBus.cpp
    Server.cpp
)

set(LILA_HEADERS
    ClangInterpreter.hpp
    Commentator.hpp
    EventBus.hpp
    Lila.hpp
    Server.hpp
)

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/lila_config.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LilaConfig.hpp"
    @ONLY
)

add_library(Lila SHARED ${LILA_SOURCES} ${LILA_HEADERS})

target_include_directories(Lila PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # For #include "Lila/..."
    ${LLVM_INCLUDE_DIRS}
)

target_precompile_headers(Lila PUBLIC ../../cmake/pch.h)

target_include_directories(Lila PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

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

target_link_libraries(Lila PRIVATE clang-cpp)
target_link_libraries(Lila PUBLIC MayaFluxLib)

set_target_properties(Lila PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
