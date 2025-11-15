set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/cmake/dependencies" CACHE PATH
    "Persistent dependencies directory")
set(FETCHCONTENT_QUIET OFF CACHE BOOL "Show FetchContent progress")
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Don't update dependencies automatically")
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL
    "Use existing dependencies without network")

if(WIN32)
    message(STATUS "=== Windows Dependency Detection ===")

    add_library(magic_enum INTERFACE IMPORTED)
    set_target_properties(magic_enum PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{MAGIC_ENUM_INCLUDE_DIR}")

    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    set_target_properties(Eigen3::Eigen PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "$ENV{EIGEN3_INCLUDE_DIR}")

    include_directories($ENV{GLM_INCLUDE_DIR} $ENV{STB_INCLUDE_DIR} $ENV{LIBXML2_INCLUDE_DIR})

    find_package(glfw3 CONFIG QUIET)
    if(glfw3_FOUND)
        message(STATUS "Using GLFW from vcpkg")
    else()
        if(DEFINED ENV{GLFW_ROOT} AND DEFINED ENV{GLFW_LIB_DIR})
            add_library(glfw SHARED IMPORTED GLOBAL)
            set_target_properties(glfw PROPERTIES
                IMPORTED_LOCATION "$ENV{GLFW_LIB_DIR}/glfw3.dll"
                IMPORTED_IMPLIB "$ENV{GLFW_LIB_DIR}/glfw3dll.lib"
                INTERFACE_INCLUDE_DIRECTORIES "$ENV{GLFW_ROOT}/include")
            message(STATUS "Using GLFW from environment: $ENV{GLFW_ROOT}")
        else()
            message(FATAL_ERROR
                    "GLFW not found via vcpkg or environment variables")
        endif()
    endif()

    find_package(RtAudio REQUIRED HINTS "$ENV{RTAUDIO_ROOT}")
    find_package(Vulkan REQUIRED)
    find_package(LLVM CONFIG REQUIRED HINTS "$ENV{LLVM_DIR}")
    find_package(Clang CONFIG REQUIRED HINTS "$ENV{Clang_DIR}")

    # FFmpeg
    foreach(component avcodec avformat avutil swresample swscale)
        string(TOUPPER ${component} comp_upper)
        add_library(FFmpeg::${component} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${component} PROPERTIES
            IMPORTED_LOCATION "$ENV{FFMPEG_ROOT}/lib/${component}.lib"
            INTERFACE_INCLUDE_DIRECTORIES "$ENV{FFMPEG_ROOT}/include")
    endforeach()

else()
    find_package(PkgConfig REQUIRED)

    if(APPLE)
        find_package(oneDPL)
        find_package(fmt REQUIRED)
        find_package(Vulkan REQUIRED)
    else()
        pkg_check_modules(Vulkan REQUIRED IMPORTED_TARGET vulkan)
    endif()

    find_package(TBB REQUIRED)
    find_package(GTest REQUIRED)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED)
    find_package(glm REQUIRED)

    pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)
    pkg_check_modules(Eigen REQUIRED IMPORTED_TARGET eigen3)
    pkg_check_modules(magic_enum REQUIRED IMPORTED_TARGET magic_enum)
    pkg_check_modules(Glfw REQUIRED IMPORTED_TARGET glfw3>=3.4)

    pkg_check_modules(LIBAVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(LIBAVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(LIBAVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(LIBSWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
    pkg_check_modules(LIBSWSCALE REQUIRED IMPORTED_TARGET libswscale)

endif()
