file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(MayaFluxTests ${TEST_SOURCES})

target_precompile_headers(MayaFluxTests PUBLIC ${CMAKE_SOURCE_DIR}/cmake/pch.h)
if(WIN32)
    target_link_libraries(MayaFluxTests PRIVATE
        MayaFluxLib
        gtest
        gtest_main
    )
else()
    target_link_libraries(MayaFluxTests PRIVATE
        MayaFluxLib
        GTest::GTest
        GTest::Main
    )
endif()

include(GoogleTest)
gtest_discover_tests(MayaFluxTests)
