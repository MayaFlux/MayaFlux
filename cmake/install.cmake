install(TARGETS MayaFluxLib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/MayaFlux/
        DESTINATION include/MayaFlux
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/MayaFlux.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFlux.pc
    @ONLY
)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/Lila.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/Lila.pc
    @ONLY
)

if(UNIX)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/MayaFlux.pc
        ${CMAKE_CURRENT_BINARY_DIR}/Lila.pc
        DESTINATION lib/pkgconfig)
endif()

install(DIRECTORY ${SHADER_OUTPUT_DIR}
    DESTINATION share/MayaFlux/
    FILES_MATCHING PATTERN "*.spv"
)

install(TARGETS Lila
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    ${CMAKE_SOURCE_DIR}/src/Lila/ClangInterpreter.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/Commentator.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/EventBus.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/Lila.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/Server.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/LiveAid.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/ServerThread.hpp
    DESTINATION include/Lila
)

install(FILES
    ${CMAKE_SOURCE_DIR}/cmake/pch.h
    ${CMAKE_SOURCE_DIR}/cmake/config.h
    DESTINATION share/MayaFlux/runtime
)

install(TARGETS lila_server RUNTIME DESTINATION bin)

# ============================================================================
# CMake Config Files
# ============================================================================

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/MayaFluxConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfig.cmake
    INSTALL_DESTINATION lib/cmake/MayaFlux
    PATH_VARS CMAKE_INSTALL_PREFIX
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfigVersion.cmake
    DESTINATION lib/cmake/MayaFlux
)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM_NAME "macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_NAME "Windows")
else()
    set(PLATFORM_NAME "Linux")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(ARCH_NAME "ARM64")
else()
    set(ARCH_NAME "x64")
endif()

install(CODE "
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_FILE=\${CMAKE_INSTALL_PREFIX}/share/MayaFlux/.version
            -DPROJECT_VERSION=${PROJECT_VERSION}
            -DPLATFORM=${PLATFORM_NAME}
            -DARCHITECTURE=${ARCH_NAME}
            -P ${CMAKE_SOURCE_DIR}/cmake/generate_version_file.cmake
    )
")

message(STATUS "Version metadata will be installed to: ${CMAKE_INSTALL_PREFIX}/share/MayaFlux/.version")

message(STATUS "Lila (static) installed to:")
message(STATUS "  - Libraries: ${CMAKE_INSTALL_PREFIX}/lib")
message(STATUS "  - Headers: ${CMAKE_INSTALL_PREFIX}/include/Lila")
message(STATUS "  - Runtime data: ${CMAKE_INSTALL_PREFIX}/share/MayaFlux/runtime")
message(STATUS "  - Executables: ${CMAKE_INSTALL_PREFIX}/bin")
message(STATUS "  - CMake config: ${CMAKE_INSTALL_PREFIX}/lib/cmake/MayaFlux")
