add_subdirectory(MayaFlux)

add_executable(MayaFlux main.cpp ${HEADERS} ${USER_SOURCES})
set_target_properties(MayaFlux PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(MayaFlux PRIVATE MayaFluxLib)
target_precompile_headers(MayaFlux PUBLIC ../cmake/pch.h)
