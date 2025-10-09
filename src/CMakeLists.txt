add_subdirectory(MayaFlux)

if(LLVM_FOUND)
    add_subdirectory(Lila)
    message(STATUS "✅ Lila live coding library enabled")
else()
    message(STATUS "⚠️  Lila live coding library disabled (LLVM not found)")
endif()

add_executable(MayaFlux main.cpp ${HEADERS} ${USER_SOURCES})
set_target_properties(MayaFlux PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(MayaFlux PRIVATE MayaFluxLib)
target_precompile_headers(MayaFlux PUBLIC ../cmake/pch.h)

add_executable(test_lila test_lila.cpp)
target_link_libraries(test_lila PRIVATE Lila)
target_link_libraries(test_lila PRIVATE Lila MayaFluxLib)

set_target_properties(test_lila PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
