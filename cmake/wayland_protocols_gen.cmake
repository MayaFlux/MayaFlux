# Wayland protocol code generation helper.
# Mirrors the approach used by KDE/extra-cmake-modules FindWaylandScanner.cmake.
#
# mf_add_wayland_protocol(<target> PROTOCOL <xml> BASENAME <name>)
#
# Runs wayland-scanner to produce:
#   ${CMAKE_CURRENT_BINARY_DIR}/wayland-<name>-client-protocol.h
#   ${CMAKE_CURRENT_BINARY_DIR}/wayland-<name>-protocol.c
#
# Both files are marked GENERATED and appended to <target> as PRIVATE sources.
# The .c output depends on the .h output so the header is always produced first.
# SKIP_AUTOMOC is set on both files to prevent moc from touching them.

function(mf_add_wayland_protocol target)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/wayland_generated")
    cmake_parse_arguments(ARGS "" "PROTOCOL;BASENAME" "" ${ARGN})

    if(NOT ARGS_PROTOCOL)
        message(FATAL_ERROR "mf_add_wayland_protocol: PROTOCOL is required")
    endif()

    if(NOT ARGS_BASENAME)
        message(FATAL_ERROR "mf_add_wayland_protocol: BASENAME is required")
    endif()

    if(NOT WaylandScanner_EXECUTABLE)
        message(FATAL_ERROR "mf_add_wayland_protocol: WaylandScanner_EXECUTABLE not set")
    endif()

    get_filename_component(_xml "${ARGS_PROTOCOL}" ABSOLUTE)
    set(_hdr "${CMAKE_CURRENT_BINARY_DIR}/wayland_generated/wayland-${ARGS_BASENAME}-client-protocol.h")
    set(_src "${CMAKE_CURRENT_BINARY_DIR}/wayland_generated/wayland-${ARGS_BASENAME}-protocol.c")

    set_source_files_properties("${_hdr}" "${_src}" PROPERTIES
        GENERATED    TRUE
        SKIP_AUTOMOC ON
    )

    add_custom_command(
        OUTPUT  "${_hdr}"
        COMMAND "${WaylandScanner_EXECUTABLE}" client-header "${_xml}" "${_hdr}"
        DEPENDS "${_xml}"
        VERBATIM
        COMMENT "wayland-scanner client-header ${ARGS_BASENAME}"
    )

    add_custom_command(
        OUTPUT  "${_src}"
        COMMAND "${WaylandScanner_EXECUTABLE}" private-code "${_xml}" "${_src}"
        DEPENDS "${_xml}" "${_hdr}"
        VERBATIM
        COMMENT "wayland-scanner private-code ${ARGS_BASENAME}"
    )

    set(_lib_name "wl_proto_${ARGS_BASENAME}")
    string(REPLACE "-" "_" _lib_name "${_lib_name}")

    add_library("${_lib_name}" OBJECT "${_src}" "${_hdr}")
    set_target_properties("${_lib_name}" PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    target_include_directories("${_lib_name}" PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/wayland_generated"
    )

    target_link_libraries("${target}" PRIVATE "${_lib_name}")
endfunction()
