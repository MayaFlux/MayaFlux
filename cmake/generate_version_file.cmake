find_package(Git QUIET)

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        RESULT_VARIABLE GIT_DIRTY
    )
    if(NOT GIT_DIRTY EQUAL 0)
        set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
        set(GIT_DESCRIBE "${GIT_DESCRIBE}-dirty")
    endif()
else()
    set(GIT_COMMIT_HASH "unknown")
    set(GIT_BRANCH "unknown")
    set(GIT_DESCRIBE "unknown")
endif()

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)

# Generate JSON format (industry standard for metadata)
file(WRITE ${OUTPUT_FILE} "{\n")
file(APPEND ${OUTPUT_FILE} "  \"version\": \"${PROJECT_VERSION}\",\n")
file(APPEND ${OUTPUT_FILE} "  \"commit\": \"${GIT_COMMIT_HASH}\",\n")
file(APPEND ${OUTPUT_FILE} "  \"branch\": \"${GIT_BRANCH}\",\n")
file(APPEND ${OUTPUT_FILE} "  \"describe\": \"${GIT_DESCRIBE}\",\n")
file(APPEND ${OUTPUT_FILE} "  \"build_time\": \"${BUILD_TIMESTAMP}\",\n")
file(APPEND ${OUTPUT_FILE} "  \"platform\": \"${PLATFORM}\",\n")
file(APPEND ${OUTPUT_FILE} "  \"architecture\": \"${ARCHITECTURE}\"\n")
file(APPEND ${OUTPUT_FILE} "}\n")

message(STATUS "Generated version file: ${OUTPUT_FILE}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Commit: ${GIT_COMMIT_HASH}")
message(STATUS "  Branch: ${GIT_BRANCH}")
