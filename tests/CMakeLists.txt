file(GLOB_RECURSE TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(MayaFluxTests ${TEST_SOURCES})

target_precompile_headers(MayaFluxTests PUBLIC ${CMAKE_SOURCE_DIR}/src/pch.h)
target_link_libraries(MayaFluxTests
    PRIVATE
    MayaFluxLib
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(MayaFluxTests)
