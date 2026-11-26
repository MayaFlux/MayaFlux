option(MAYAFLUX_REFLECT_BUILD
    "Run mayaflux_reflect and mayaflux_signatures as build steps before MayaFluxLib" ON)

function(mayaflux_reflect_sources target unit)
    if(NOT MAYAFLUX_REFLECT_BUILD OR NOT CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
        return()
    endif()

    set(scan_flags "")
    if(MSVC)
        list(APPEND scan_flags --extra-arg=-Xclang --extra-arg=-std=c++23)
    else()
        list(APPEND scan_flags --extra-arg=-Wno-ignored-gch)
    endif()

    set(stamps "")
    foreach(header IN LISTS ARGN)
        if(NOT header MATCHES "\\.(h|hh|hpp|hxx)$" OR header MATCHES "\\.(describe|signatures)\\.[^.]+$")
            continue()
        endif()

        get_filename_component(dir "${header}" DIRECTORY)
        get_filename_component(stem "${header}" NAME_WLE)
        file(RELATIVE_PATH rel "${CMAKE_SOURCE_DIR}/src" "${header}")

        file(STRINGS "${header}" request REGEX "^[ \t;}]*MF_DESCRIBE[ \t]*\\(" LIMIT_COUNT 1)
        if(request)
            set(sidecar "${dir}/${stem}.describe.hpp")
            set(depfile "${CMAKE_BINARY_DIR}/reflect/${rel}.d")
            set(stamp "${CMAKE_BINARY_DIR}/reflect/${rel}.stamp")
            add_custom_command(
                OUTPUT "${stamp}"
                BYPRODUCTS "${sidecar}"
                COMMAND mayaflux_reflect --build "${header}"
                    -p "${CMAKE_BINARY_DIR}"
                    --translation-unit "${unit}"
                    ${scan_flags}
                    --depfile "${depfile}"
                    --depfile-target "${stamp}"
                COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
                DEPENDS "${header}" mayaflux_reflect
                DEPFILE "${depfile}"
                COMMENT "Describing ${rel}"
                VERBATIM
            )
            list(APPEND stamps "${stamp}")
        endif()

        file(STRINGS "${header}" function_request REGEX "^[ \t;}]*MF_DESCRIBE_FUNCTION[ \t]*\\(" LIMIT_COUNT 1)
        if(function_request)
            set(sidecar "${dir}/${stem}.signatures.hpp")
            set(depfile "${CMAKE_BINARY_DIR}/reflect/${rel}.signatures.d")
            set(stamp "${CMAKE_BINARY_DIR}/reflect/${rel}.signatures.stamp")
            add_custom_command(
                OUTPUT "${stamp}"
                BYPRODUCTS "${sidecar}"
                COMMAND mayaflux_signatures --functions --build "${header}"
                    -p "${CMAKE_BINARY_DIR}"
                    --translation-unit "${unit}"
                    ${scan_flags}
                    --depfile "${depfile}"
                    --depfile-target "${stamp}"
                COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
                DEPENDS "${header}" mayaflux_signatures
                DEPFILE "${depfile}"
                COMMENT "Describing functions in ${rel}"
                VERBATIM
            )
            list(APPEND stamps "${stamp}")
        endif()
    endforeach()

    if(stamps)
        add_custom_target(${target}_reflect DEPENDS ${stamps})
        add_dependencies(${target} ${target}_reflect)
    endif()
endfunction()

if(NOT MAYAFLUX_REFLECT_BUILD)
    return()
endif()

if(NOT CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    message(STATUS "Reflection build steps skipped: ${CMAKE_GENERATOR} writes no compile_commands.json")
    return()
endif()

set(_mf_src "${CMAKE_SOURCE_DIR}/src")
set(_mf_unit "${_mf_src}/MayaFlux/API/Proxy/Creator.cpp")
set(_mf_work "${CMAKE_BINARY_DIR}/reflect")

set(_mf_scan_flags "")
if(MSVC)
    list(APPEND _mf_scan_flags --extra-arg=-Xclang --extra-arg=-std=c++23)
else()
    list(APPEND _mf_scan_flags --extra-arg=-Wno-ignored-gch)
endif()

file(GLOB_RECURSE _mf_headers "${_mf_src}/MayaFlux/*.hpp")

set(_mf_sidecars "")
set(_mf_stamps "")
foreach(_header IN LISTS _mf_headers)
    if(_header MATCHES "\\.(describe|signatures)\\.hpp$")
        continue()
    endif()

    get_filename_component(_mf_dir "${_header}" DIRECTORY)
    get_filename_component(_mf_stem "${_header}" NAME_WLE)
    file(RELATIVE_PATH _mf_rel "${_mf_src}" "${_header}")

    file(STRINGS "${_header}" _mf_request REGEX "^[ \t;}]*MF_DESCRIBE[ \t]*\\(" LIMIT_COUNT 1)
    if(_mf_request)
        set(_mf_sidecar "${_mf_dir}/${_mf_stem}.describe.hpp")
        set(_mf_depfile "${_mf_work}/${_mf_rel}.d")
        set(_mf_stamp "${_mf_work}/${_mf_rel}.stamp")

        add_custom_command(
            OUTPUT "${_mf_stamp}"
            BYPRODUCTS "${_mf_sidecar}"
            COMMAND mayaflux_reflect --build "${_header}"
                -p "${CMAKE_BINARY_DIR}"
                --translation-unit "${_mf_unit}"
                ${_mf_scan_flags}
                --depfile "${_mf_depfile}"
                --depfile-target "${_mf_stamp}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_mf_stamp}"
            DEPENDS "${_header}" mayaflux_reflect
            DEPFILE "${_mf_depfile}"
            COMMENT "Describing ${_mf_rel}"
            VERBATIM
        )
        list(APPEND _mf_sidecars "${_mf_sidecar}")
        list(APPEND _mf_stamps "${_mf_stamp}")
    endif()

    file(STRINGS "${_header}" _mf_function_request REGEX "^[ \t;}]*MF_DESCRIBE_FUNCTION[ \t]*\\(" LIMIT_COUNT 1)
    if(_mf_function_request)
        set(_mf_sidecar "${_mf_dir}/${_mf_stem}.signatures.hpp")
        set(_mf_depfile "${_mf_work}/${_mf_rel}.signatures.d")
        set(_mf_stamp "${_mf_work}/${_mf_rel}.signatures.stamp")

        add_custom_command(
            OUTPUT "${_mf_stamp}"
            BYPRODUCTS "${_mf_sidecar}"
            COMMAND mayaflux_signatures --functions --build "${_header}"
                -p "${CMAKE_BINARY_DIR}"
                --translation-unit "${_mf_unit}"
                ${_mf_scan_flags}
                --depfile "${_mf_depfile}"
                --depfile-target "${_mf_stamp}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_mf_stamp}"
            DEPENDS "${_header}" mayaflux_signatures
            DEPFILE "${_mf_depfile}"
            COMMENT "Describing functions in ${_mf_rel}"
            VERBATIM
        )
        list(APPEND _mf_sidecars "${_mf_sidecar}")
        list(APPEND _mf_stamps "${_mf_stamp}")
    endif()
endforeach()

if(_mf_sidecars)
    add_custom_target(mayaflux_reflect_sidecars DEPENDS ${_mf_stamps})
    add_dependencies(MayaFluxLib mayaflux_reflect_sidecars)
endif()

set(_mf_proxy "${_mf_src}/MayaFlux/API/Proxy")
set(_mf_generated "${_mf_proxy}/Generated")
set(_mf_creator_stamp "${_mf_work}/creator.stamp")
set(_mf_creator_files
    "${_mf_generated}/CreatorIncludes.hpp"
    "${_mf_generated}/CreatorClasses.hpp"
    "${_mf_generated}/CreatorDeclarations.inc"
    "${_mf_generated}/CreatorDefinitions.inc"
)

add_custom_command(
    OUTPUT "${_mf_creator_stamp}"
    BYPRODUCTS ${_mf_creator_files}
    COMMAND mayaflux_signatures --build
        --root "${CMAKE_SOURCE_DIR}"
        -p "${CMAKE_BINARY_DIR}"
        ${_mf_scan_flags}
        --depfile "${_mf_work}/creator.d"
        --depfile-target "${_mf_creator_stamp}"
    COMMAND ${CMAKE_COMMAND} -E touch "${_mf_creator_stamp}"
    DEPENDS "${_mf_proxy}/Registry.hpp" mayaflux_signatures
    DEPFILE "${_mf_work}/creator.d"
    COMMENT "Generating Creator methods"
    VERBATIM
)
add_custom_target(mayaflux_creator_methods DEPENDS "${_mf_creator_stamp}")
if(_mf_sidecars)
    add_dependencies(mayaflux_creator_methods mayaflux_reflect_sidecars)
endif()
add_dependencies(MayaFluxLib mayaflux_creator_methods)
