add_subdirectory(MayaFlux)

if(LLVM_FOUND)
    add_subdirectory(Lila)
    message(STATUS "✅ Lila live coding library enabled")

    add_executable(lila_server lila_server.cpp)
    target_link_libraries(lila_server PRIVATE Lila)

    set_target_properties(lila_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
else()
    message(STATUS "⚠️  Lila live coding library disabled (LLVM not found)")
endif()

add_executable(MayaFlux main.cpp ${HEADERS} ${USER_SOURCES})
set_target_properties(MayaFlux PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(MayaFlux PRIVATE MayaFluxLib)
target_precompile_headers(MayaFlux PUBLIC ../cmake/pch.h)

