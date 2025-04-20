cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MayaFlux)
find_package(PkgConfig REQUIRED)
pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)

find_package(GTest REQUIRED)
enable_testing()

file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

list(FILTER SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_library(MayaFluxLib ${SOURCES})
target_include_directories(MayaFluxLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(MayaFluxLib PUBLIC PkgConfig::RtAudio)

add_compile_options (
 -Wall
 -Wextra
 -Wpedantic
)

add_executable(MayaFlux src/main.cpp)
target_link_libraries(MayaFlux PRIVATE MayaFluxLib)

add_subdirectory(tests)

execute_process (
 COMMAND ${CMAKE_COMMAND} -E create_symlink
  ${CMAKE_BINARY_DIR}/compile_commands.json
  ${CMAKE_SOURCE_DIR}/compile_commands.json
)

install(TARGETS MayaFlux DESTINATION /usr/local/bin)
