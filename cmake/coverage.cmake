execute_process(
    COMMAND "${TEST_EXECUTABLE}" --gtest_color=yes
    WORKING_DIRECTORY "${BUILD_ROOT}"
    RESULT_VARIABLE test_result
)
message(STATUS "Tests exited with ${test_result}; writing coverage regardless")

file(MAKE_DIRECTORY "${REPORT_DIR}")

set(gcovr_args
    --root "${SOURCE_ROOT}"
    --object-directory "${BUILD_ROOT}"
    --filter "${FILTER}"
    --gcov-ignore-parse-errors negative_hits.warn_once_per_file
    --print-summary
    --txt "${REPORT_DIR}/summary.txt"
    --html-details "${REPORT_DIR}/index.html")
if(GCOV_EXECUTABLE)
    list(APPEND gcovr_args --gcov-executable "${GCOV_EXECUTABLE}")
endif()

execute_process(
    COMMAND "${GCOVR}" ${gcovr_args}
    WORKING_DIRECTORY "${SOURCE_ROOT}"
    RESULT_VARIABLE gcovr_result
)
if(NOT gcovr_result EQUAL 0)
    message(FATAL_ERROR "gcovr failed with ${gcovr_result}")
endif()

message(STATUS "Coverage report: ${REPORT_DIR}/index.html")
