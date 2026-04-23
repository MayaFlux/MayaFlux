include_guard(GLOBAL)

add_library(MayaFluxHost SHARED
    ${CMAKE_CURRENT_LIST_DIR}/Live/Runtime.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Live/Runtime.hpp
)

set_target_properties(MayaFluxHost PROPERTIES
    VERSION     ${PROJECT_VERSION}
    SOVERSION   ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

if(WIN32)
    target_compile_definitions(MayaFluxHost PRIVATE MAYAFLUX_HOST_EXPORTS)
    target_compile_options(MayaFluxHost PRIVATE /MD)
endif()

target_include_directories(MayaFluxHost PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../
)

target_precompile_headers(MayaFluxHost PRIVATE ${CMAKE_SOURCE_DIR}/cmake/pch.h)

if(WIN32)
    target_link_libraries(MayaFluxHost PRIVATE Lila Transitive_MD)
    target_link_options(MayaFluxHost PRIVATE /NODEFAULTLIB:libcmt /DEFAULTLIB:msvcrt)
else()
    target_link_libraries(MayaFluxHost PUBLIC Lila Transitive)
endif()
