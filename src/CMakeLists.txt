add_subdirectory(MayaFlux)

add_subdirectory(Lila)

add_executable(lila_server lila_server.cpp)
target_link_libraries(lila_server PRIVATE Lila)

set_target_properties(lila_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    set_target_properties(lila_server PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin;$ENV{PATH}"
    )
else()
    set_target_properties(lila_server PROPERTIES
        INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()

add_executable(MayaFlux main.cpp ${HEADERS} ${USER_SOURCES})
set_target_properties(MayaFlux PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_link_libraries(MayaFlux PRIVATE MayaFluxLib)
target_precompile_headers(MayaFlux PUBLIC ../cmake/pch.h)
