option(MAYAFLUX_REFLECT_BUILD
    "Run mayaflux_reflect and mayaflux_signatures as build steps before MayaFluxLib" ON)

set(_mf_scan_flags "")
if(MSVC)
    list(APPEND _mf_scan_flags --extra-arg=-Xclang --extra-arg=-std=c++23)
else()
    list(APPEND _mf_scan_flags --extra-arg=-Wno-ignored-gch)
endif()
list(APPEND _mf_scan_flags --prefix-header "${CMAKE_SOURCE_DIR}/cmake/pch.h")

function(mayaflux_reflect_step target unit kind headers)
    if(NOT headers)
        return()
    endif()

    if(kind STREQUAL "describe")
        set(command mayaflux_reflect --build)
        set(tool mayaflux_reflect)
        set(suffix describe)
    else()
        set(command mayaflux_signatures --functions --build)
        set(tool mayaflux_signatures)
        set(suffix signatures)
    endif()

    set(sidecars "")
    set(listing "")
    foreach(header IN LISTS headers)
        get_filename_component(dir "${header}" DIRECTORY)
        get_filename_component(stem "${header}" NAME_WLE)
        list(APPEND sidecars "${dir}/${stem}.${suffix}.hpp")
        string(APPEND listing "\"${header}\"\n")
    endforeach()

    set(work "${CMAKE_BINARY_DIR}/reflect/${target}.${suffix}")
    file(CONFIGURE OUTPUT "${work}.rsp" CONTENT "${listing}")

    add_custom_command(
        OUTPUT "${work}.stamp"
        BYPRODUCTS ${sidecars}
        COMMAND ${command} "@${work}.rsp"
            -p "${CMAKE_BINARY_DIR}"
            --translation-unit "${unit}"
            ${_mf_scan_flags}
            --depfile "${work}.d"
            --depfile-target "${work}.stamp"
        COMMAND ${CMAKE_COMMAND} -E touch "${work}.stamp"
        DEPENDS ${headers} "${work}.rsp" ${tool}
        DEPFILE "${work}.d"
        COMMENT "Reflecting ${suffix} headers of ${target}"
        VERBATIM
    )
    add_custom_target(${target}_reflect_${suffix} DEPENDS "${work}.stamp")
    add_dependencies(${target} ${target}_reflect_${suffix})
endfunction()

function(mayaflux_reflect_sources target unit)
    if(NOT MAYAFLUX_REFLECT_BUILD OR NOT CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
        return()
    endif()

    set(candidates ${ARGN})
    list(REMOVE_DUPLICATES candidates)

    set(described "")
    set(functions "")
    foreach(header IN LISTS candidates)
        if(NOT header MATCHES "\\.(h|hh|hpp|hxx)$" OR header MATCHES "\\.(describe|signatures)\\.[^.]+$")
            continue()
        endif()

        file(STRINGS "${header}" request REGEX "^[ \t;}]*MF_DESCRIBE[ \t]*\\(" LIMIT_COUNT 1)
        if(request)
            list(APPEND described "${header}")
        endif()

        file(STRINGS "${header}" function_request REGEX "^[ \t;}]*MF_DESCRIBE_FUNCTION[ \t]*\\(" LIMIT_COUNT 1)
        if(function_request)
            list(APPEND functions "${header}")
        endif()
    endforeach()

    mayaflux_reflect_step(${target} "${unit}" describe "${described}")
    mayaflux_reflect_step(${target} "${unit}" function "${functions}")
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

file(GLOB_RECURSE _mf_headers "${_mf_src}/MayaFlux/*.hpp")
mayaflux_reflect_sources(MayaFluxLib "${_mf_unit}" ${_mf_headers})

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
foreach(_mf_step describe signatures)
    if(TARGET MayaFluxLib_reflect_${_mf_step})
        add_dependencies(mayaflux_creator_methods MayaFluxLib_reflect_${_mf_step})
    endif()
endforeach()
add_dependencies(MayaFluxLib mayaflux_creator_methods)
if(TARGET MayaFluxHeavy)
    add_dependencies(MayaFluxHeavy mayaflux_creator_methods)
endif()
