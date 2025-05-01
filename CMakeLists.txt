cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MayaFlux VERSION 0.1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(PkgConfig REQUIRED)
pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)

if(UNIX)
    find_package(UUID QUIET)
    if(NOT UUID_FOUND)
        pkg_check_modules(UUID REQUIRED uuid)
    endif()
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MayaFlux/version.h
    @ONLY
)

find_package(GTest REQUIRED)
enable_testing()

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_library(MayaFluxLib ${SOURCES})

add_library(MayaFluxLibShared SHARED ${SOURCES})
set_target_properties(MayaFluxLibShared PROPERTIES
    OUTPUT_NAME MayaFluxLib
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(MayaFluxLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${NLOHMANN_JSON_INCLUDE_DIRS})
target_include_directories(MayaFluxLibShared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${NLOHMANN_JSON_INCLUDE_DIRS})

target_link_libraries(MayaFluxLib PUBLIC PkgConfig::RtAudio PkgConfig::SNDFILE)
if(UNIX)
    target_link_libraries(MayaFluxLib PUBLIC ${UUID_LIBRARIES})
endif()
target_link_libraries(MayaFluxLibShared PUBLIC PkgConfig::RtAudio PkgConfig::SNDFILE)
if(UNIX)
    target_link_libraries(MayaFluxLibShared PUBLIC ${UUID_LIBRARIES})
endif()

add_executable(MayaFlux src/main.cpp)
target_link_libraries(MayaFlux PRIVATE MayaFluxLib)

if(WIN32)
    target_compile_definitions(MayaFluxLibShared
        PRIVATE MAYAFLUX_EXPORTS
        INTERFACE MAYAFLUX_IMPORTS)
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)

add_subdirectory(tests)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
)

install(TARGETS MayaFluxLib MayaFluxLibShared DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(TARGETS MayaFlux DESTINATION bin)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mayaflux.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc
        DESTINATION lib/pkgconfig)

message(STATUS "MayaFlux configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
