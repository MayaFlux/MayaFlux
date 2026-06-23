install(TARGETS MayaFluxLib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/MayaFlux/
        DESTINATION include/MayaFlux
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inl")

if(UNIX AND NOT APPLE)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/src/MayaFlux/wayland_generated/"
        DESTINATION include/MayaFlux/Core/Backends/Windowing/Wayland
        FILES_MATCHING PATTERN "*.h"
    )
endif()

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
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/MayaFluxHost.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxHost.pc
    @ONLY
)

if(UNIX)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxHost.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

install(DIRECTORY ${SHADER_OUTPUT_DIR}
    DESTINATION share/MayaFlux/
    FILES_MATCHING PATTERN "*.spv"
)

install(TARGETS Lila
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ${CMAKE_SOURCE_DIR}/src/Lila/ClangInterpreter.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/Commentator.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/EventBus.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/Lila.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/Server.hpp
    ${CMAKE_SOURCE_DIR}/src/Lila/LiveAid.hpp
    DESTINATION include/Lila
)

install(FILES
    ${CMAKE_SOURCE_DIR}/cmake/pch_minimal.h
    ${CMAKE_SOURCE_DIR}/cmake/pch.h
    ${CMAKE_SOURCE_DIR}/cmake/config.h
    DESTINATION share/MayaFlux/runtime
)

install(TARGETS lila_server RUNTIME DESTINATION bin)

install(TARGETS MayaFluxHost
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ${CMAKE_SOURCE_DIR}/src/Host/Live/Runtime.hpp
    DESTINATION include/MayaFlux/Host/Live
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/tinyexr/
    DESTINATION include/MayaFlux/thirdparty/tinyexr
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hh"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/magic_enum/
    DESTINATION include/MayaFlux/thirdparty/magic_enum
    FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS miniz
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ============================================================================
# CMake Config Files
# ============================================================================

if(DEFINED ENV{VCPKG_ROOT})
    file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}/installed" VCPKG_INSTALLED_DIR)
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/MayaFluxConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MayaFluxConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MayaFlux
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
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MayaFlux
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

find_package(Git QUIET)

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_DIRTY
    )
    if(NOT GIT_DIRTY EQUAL 0)
        set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
        set(GIT_DESCRIBE "${GIT_DESCRIBE}-dirty")
    endif()
else()
    set(GIT_COMMIT_HASH "unknown")
    set(GIT_BRANCH "unknown")
    set(GIT_DESCRIBE "unknown")
endif()

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)

file(WRITE "${CMAKE_BINARY_DIR}/generated_version.json"
"{\n\
  \"version\": \"${PROJECT_VERSION}\",\n\
  \"commit\": \"${GIT_COMMIT_HASH}\",\n\
  \"branch\": \"${GIT_BRANCH}\",\n\
  \"describe\": \"${GIT_DESCRIBE}\",\n\
  \"build_time\": \"${BUILD_TIMESTAMP}\",\n\
  \"platform\": \"${PLATFORM_NAME}\",\n\
  \"architecture\": \"${ARCH_NAME}\"\n\
}\n"
)

install(FILES "${CMAKE_BINARY_DIR}/generated_version.json"
    DESTINATION share/MayaFlux
    RENAME .version
)

message(STATUS "Version metadata will be installed to: ${CMAKE_INSTALL_PREFIX}/share/MayaFlux/.version")

message(STATUS "Lila (static) installed to:")
message(STATUS "  - Libraries: ${CMAKE_INSTALL_PREFIX}/lib")
message(STATUS "  - Headers: ${CMAKE_INSTALL_PREFIX}/include/Lila")
message(STATUS "  - Runtime data: ${CMAKE_INSTALL_PREFIX}/share/MayaFlux/runtime")
message(STATUS "  - Executables: ${CMAKE_INSTALL_PREFIX}/bin")
message(STATUS "  - CMake config: ${CMAKE_INSTALL_PREFIX}/lib/cmake/MayaFlux")
