set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4)
else()
    if(APPLE AND CMAKE_SYSTEM_VERSION VERSION_LESS "15.0")
        add_compile_options(
            -fpermissive
            -Wno-error
            -Wno-unused-lambda-capture
            -Wno-c++20-extensions
            -Wno-c++23-extensions
            -Wno-unused-variable
            -Wno-unused-parameter
            -Wno-extra
            -Wno-pedantic
        )
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++2b")

        message(STATUS
                "Applied Apple Clang 15 (macOS 14) compatibility workarounds")
    else()
        add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )
    endif()
endif()
