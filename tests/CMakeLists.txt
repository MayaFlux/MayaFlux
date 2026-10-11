file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

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

include(GoogleTest)
gtest_discover_tests(MayaFluxTests)
