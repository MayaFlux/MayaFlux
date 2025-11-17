install(TARGETS MayaFluxLib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/MayaFlux/
        DESTINATION include/MayaFlux
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mayaflux.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc
    @ONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lila.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/lila.pc
    @ONLY
)

if(UNIX)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc
        ${CMAKE_CURRENT_BINARY_DIR}/lila.pc
        DESTINATION lib/pkgconfig)
endif()

install(DIRECTORY ${SHADER_OUTPUT_DIR}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/mayaflux/
    FILES_MATCHING PATTERN "*.spv"
)

install(TARGETS Lila
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/ClangInterpreter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/Commentator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/EventBus.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/Lila.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/Server.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/LiveAid.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/ServerThread.hpp
    DESTINATION include/Lila
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/pch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h
    DESTINATION share/lila/runtime
)

install(TARGETS lila_server RUNTIME DESTINATION bin)

# ============================================================================
# CMake Config Files
# ============================================================================

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MayaFluxConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfig.cmake
    @ONLY
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# if(UNIX)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfigVersion.cmake
    DESTINATION lib/cmake/MayaFlux
)
# endif()

message(STATUS "Lila (static) installed to:")
message(STATUS "  - Libraries: ${CMAKE_INSTALL_PREFIX}/lib")
message(STATUS "  - Headers: ${CMAKE_INSTALL_PREFIX}/include/Lila")
message(STATUS "  - Runtime data: ${CMAKE_INSTALL_PREFIX}/share/lila/runtime")
message(STATUS "  - Executables: ${CMAKE_INSTALL_PREFIX}/bin")
message(STATUS "  - CMake config: ${CMAKE_INSTALL_PREFIX}/lib/cmake/MayaFlux")
