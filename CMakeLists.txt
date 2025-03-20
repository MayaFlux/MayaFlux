cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project( MayaFlux)
# find_package(RtAudio REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)

# file(
#  GLOB_RECURSE SOURCES
#  src/*.cpp
#  src/**/*.cpp
#  src/**/**/*.cpp
# )

file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)


add_compile_options (
 -Wall
 -Wextra
 -Wpedantic
)

add_executable(MayaFlux ${SOURCES})

execute_process (
 COMMAND ${CMAKE_COMMAND} -E create_symlink
  ${CMAKE_BINARY_DIR}/compile_commands.json
  ${CMAKE_SOURCE_DIR}/compile_commands.json
)

target_include_directories(MayaFlux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# target_link_libraries(MayaFlux PUBLIC RtAudio::rtaudio)
target_link_libraries(MayaFlux PUBLIC PkgConfig::RtAudio)

install(TARGETS MayaFlux DESTINATION /usr/local/bin)
