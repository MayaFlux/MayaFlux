set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -pipe
    )
    if(APPLE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.6" CACHE STRING
            "Minimum macOS version" FORCE)
    endif()

elseif(MSVC)
    add_compile_options(
        /W4
        /MP
    )
endif()
