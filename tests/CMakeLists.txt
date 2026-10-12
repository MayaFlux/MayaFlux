file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

if(APPLE)
    file(GLOB_RECURSE TEST_OBJCXX_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.mm
    )
    list(APPEND TEST_SOURCES ${TEST_OBJCXX_SOURCES})
    set_source_files_properties(${TEST_OBJCXX_SOURCES}
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-include;${CMAKE_SOURCE_DIR}/cmake/pch.h;-fobjc-arc"
    )
endif()

add_executable(MayaFluxTests ${TEST_SOURCES})


set_target_properties(MayaFluxTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

find_package(GTest REQUIRED)

target_precompile_headers(MayaFluxTests PUBLIC ${CMAKE_SOURCE_DIR}/cmake/pch.h)

if(MSVC)
    set_property(TARGET MayaFluxTests PROPERTY
        MSVC_RUNTIME_LIBRARY MultiThreadedDLL
    )
endif()

target_link_libraries(MayaFluxTests PRIVATE
    MayaFluxLib
    GTest::gtest
    GTest::gtest_main
)

if(UNIX AND NOT APPLE AND MAYAFLUX_AUDIO_BACKEND STREQUAL "PIPEWIRE_BACKEND")
    target_link_libraries(MayaFluxTests PRIVATE PkgConfig::PIPEWIRE)
endif()

if(UNIX AND NOT APPLE AND MAYAFLUX_WINDOWING_BACKEND STREQUAL "WAYLAND_BACKEND")
    target_include_directories(MayaFluxTests PRIVATE
        "${CMAKE_BINARY_DIR}/src/MayaFlux/wayland_generated"
    )
endif()

if(UNIX AND NOT APPLE)
    target_include_directories(MayaFluxTests PRIVATE ${DBUS_INCLUDE_DIRS})
    target_link_libraries(MayaFluxTests PRIVATE ${DBUS_LIBRARIES})
endif()

if(APPLE AND MAYAFLUX_AUDIO_BACKEND STREQUAL "COREAUDIO_BACKEND")
    target_link_libraries(MayaFluxTests PRIVATE
        "-framework CoreAudio"
        "-framework AudioUnit"
        "-framework AudioToolbox"
    )
endif()

if(APPLE AND MAYAFLUX_MIDI_BACKEND STREQUAL "COREMIDI_BACKEND")
    target_link_libraries(MayaFluxTests PRIVATE
        "-framework CoreMIDI"
        "-framework CoreFoundation"
    )
endif()

if(APPLE AND MAYAFLUX_WINDOWING_BACKEND STREQUAL "COCOA_BACKEND")
    target_link_libraries(MayaFluxTests PRIVATE
        "-framework AppKit"
        "-framework QuartzCore"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework UniformTypeIdentifiers"
    )
endif()

include(GoogleTest)
gtest_discover_tests(MayaFluxTests)
