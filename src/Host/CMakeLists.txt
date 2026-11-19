add_library(MayaFluxHost SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/Session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Session.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies.hpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

target_precompile_headers(MayaFluxHost PRIVATE ${CMAKE_SOURCE_DIR}/cmake/pch_minimal.h)

if(WIN32)
    target_link_libraries(MayaFluxHost PRIVATE Lila Transitive_Free Transitive_Live)
    target_link_options(MayaFluxHost PRIVATE /NODEFAULTLIB:libcmt /DEFAULTLIB:msvcrt)
else()
    target_link_libraries(MayaFluxHost PUBLIC Lila Transitive_Free Transitive_Live)
endif()
