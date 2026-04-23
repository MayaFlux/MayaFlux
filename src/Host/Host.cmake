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
endif()

target_include_directories(MayaFluxHost PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../
)

target_precompile_headers(MayaFluxHost REUSE_FROM Lila)

if(WIN32)
    target_link_libraries(MayaFluxHost PUBLIC Lila Transitive_MT)
else()
    target_link_libraries(MayaFluxHost PUBLIC Lila Transitive)
endif()
