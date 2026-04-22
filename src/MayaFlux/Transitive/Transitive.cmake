include_guard(GLOBAL)

file(GLOB_RECURSE TRANSITIVE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

file(GLOB_RECURSE TRANSITIVE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

if(WIN32)
    add_library(Transitive_MD OBJECT ${TRANSITIVE_SOURCES} ${TRANSITIVE_HEADERS})
    add_library(Transitive_MT OBJECT ${TRANSITIVE_SOURCES} ${TRANSITIVE_HEADERS})

    foreach(_t Transitive_MD Transitive_MT)
        set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        target_include_directories(${_t} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../..)
        target_precompile_headers(${_t} PRIVATE ${CMAKE_SOURCE_DIR}/cmake/pch.h)
        target_link_libraries(${_t} PUBLIC glm::glm magic_enum::magic_enum)
    endforeach()

    target_compile_options(Transitive_MD PRIVATE /MD)
    target_compile_options(Transitive_MT PRIVATE /MT)

    add_library(Transitive ALIAS Transitive_MD)

    target_compile_definitions(Transitive_MD PRIVATE MAYAFLUX_EXPORTS)
else()
    add_library(Transitive OBJECT ${TRANSITIVE_SOURCES} ${TRANSITIVE_HEADERS})

    set_target_properties(Transitive PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    target_include_directories(Transitive PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../..
    )

    target_precompile_headers(Transitive PRIVATE ${CMAKE_SOURCE_DIR}/cmake/pch.h)

    target_link_libraries(Transitive PUBLIC PkgConfig::magic_enum)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(Transitive PUBLIC PkgConfig::Fontconfig)
    endif()
endif()
