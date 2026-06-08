add_subdirectory(MayaFlux)

add_subdirectory(Lila)

include(Host/Host.cmake)

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

if(WIN32 AND MAYAFLUX_DEV)
    add_custom_target(copy_runtime_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:MayaFluxLib>
            $<TARGET_FILE:Lila>
            $<TARGET_FILE:MayaFluxHost>
            ${CMAKE_BINARY_DIR}
        DEPENDS MayaFluxLib Lila MayaFluxHost
        COMMENT "Copying runtime DLLs to binary directory"
    )
    add_dependencies(lila_server copy_runtime_dlls)
endif()

if(NOT MAYAFLUX_BUILD_PROJECT)
    return()
endif()

option(MAYAFLUX_CONFIG_OVERRIDE
       "JSON config file takes precedence over settings()" OFF)

file(GLOB_RECURSE EXAMPLES
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp"
)
if(EXAMPLES)
    list(APPEND USER_SOURCES ${EXAMPLES})
endif()

add_executable(project_launcher main.cpp ${HEADERS} ${USER_SOURCES})
set_target_properties(project_launcher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(WIN32)
    add_dependencies(project_launcher copy_runtime_dlls)
    set_target_properties(project_launcher PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=$<TARGET_FILE_DIR:MayaFluxLib>;$<TARGET_FILE_DIR:MayaFluxHost>;$<TARGET_FILE_DIR:Lila>;$ENV{PATH}"
    )
endif()

target_link_libraries(project_launcher PRIVATE MayaFluxLib MayaFluxHost)
target_precompile_headers(project_launcher
    PUBLIC ${CMAKE_SOURCE_DIR}/cmake/pch.h)

if(TARGET compile_example_shaders)
    add_dependencies(project_launcher compile_example_shaders)
    target_compile_definitions(project_launcher PRIVATE
        MAYAFLUX_EXAMPLE_SHADER_DIR="${CMAKE_SOURCE_DIR}/src/examples/shaders"
    )
endif()
