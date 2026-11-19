include_guard(GLOBAL)

file(GLOB_RECURSE TRANSITIVE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

file(GLOB_RECURSE TRANSITIVE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

add_library(Transitive OBJECT ${TRANSITIVE_SOURCES} ${TRANSITIVE_HEADERS})

set_target_properties(Transitive PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(Transitive PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../..
)

target_precompile_headers(Transitive PRIVATE ${CMAKE_SOURCE_DIR}/cmake/pch_minimal.h)

if(WIN32)
    target_link_libraries(Transitive PUBLIC magic_enum nlohmann_json::nlohmann_json)
    target_compile_options(Transitive PRIVATE /MD)
    target_compile_definitions(Transitive PRIVATE MAYAFLUX_EXPORTS)
else()
    target_link_libraries(Transitive PUBLIC magic_enum PkgConfig::nlohmann_json)
endif()

add_library(Transitive_Free INTERFACE)

target_sources(Transitive_Free INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../..
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/Facet/Facets.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Memory/FlatMap.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Memory/PendingQueue.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Memory/RingBuffer.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Memory/SeqLock.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Memory/SnapshotCell.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Memory/SweepList.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Parallel/AsyncGroup.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Parallel/Dispatch.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Platform/Acquisition.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Platform/HostEnvironment.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Platform/Process.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Platform/Toolchain.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Platform/UnixSocket.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Protocol/BinaryBuffer.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Protocol/BitField.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Protocol/Framing.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Protocol/HostPort.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Reflect/Describe.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Reflect/DescribedBinding.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Reflect/Mirror.hpp
        ${CMAKE_CURRENT_LIST_DIR}/Reflect/TypeInfo.hpp
)

target_precompile_headers(Transitive_Free INTERFACE ${CMAKE_SOURCE_DIR}/cmake/pch_minimal.h)

add_library(Transitive_Live OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/Memory/LiveArena.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Memory/LiveArena.hpp
    ${CMAKE_CURRENT_LIST_DIR}/Memory/Persist.hpp
)

set_target_properties(Transitive_Live PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(Transitive_Live PUBLIC Transitive_Free)

if(WIN32)
    target_compile_options(Transitive_Live PRIVATE /MD)
    target_compile_definitions(Transitive_Live PRIVATE MAYAFLUX_EXPORTS)
endif()
