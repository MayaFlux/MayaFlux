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

target_precompile_headers(Transitive PRIVATE ${CMAKE_SOURCE_DIR}/cmake/pch.h)

if(WIN32)
    target_link_libraries(Transitive PUBLIC magic_enum::magic_enum)
    target_compile_definitions(Transitive PRIVATE MAYAFLUX_EXPORTS)
else()
    target_link_libraries(Transitive PUBLIC PkgConfig::magic_enum)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(Transitive PUBLIC PkgConfig::Fontconfig)
    endif()
endif()
