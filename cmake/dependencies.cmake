set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/cmake/dependencies" CACHE PATH
    "Persistent dependencies directory")
set(FETCHCONTENT_QUIET OFF CACHE BOOL "Show FetchContent progress")
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Don't update dependencies automatically")
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL
    "Use existing dependencies without network")

if(WIN32)
    add_library(magic_enum INTERFACE IMPORTED)
    set_target_properties(magic_enum PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MAGIC_ENUM_INCLUDE_DIR}"
    )

    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    set_target_properties(Eigen3::Eigen PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}"
    )

    include_directories(${GLM_INCLUDE_DIR})

    add_library(glfw SHARED IMPORTED)
    set_target_properties(glfw PROPERTIES
        IMPORTED_LOCATION "${GLFW_DLL}"
        IMPORTED_IMPLIB "${GLFW_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
    )

    find_package(RtAudio REQUIRED)
    find_package(Vulkan REQUIRED)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED )


    add_library(FFmpeg::avcodec UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::avcodec PROPERTIES
        IMPORTED_LOCATION "${AVCODEC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AVCODEC_INCLUDE_DIR}"
    )
    add_library(FFmpeg::avformat UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::avformat PROPERTIES
        IMPORTED_LOCATION "${AVFORMAT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AVFORMAT_INCLUDE_DIR}"
    )
    add_library(FFmpeg::avutil UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::avutil PROPERTIES
        IMPORTED_LOCATION "${AVUTIL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AVUTIL_INCLUDE_DIR}"
    )
    add_library(FFmpeg::swresample UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::swresample PROPERTIES
        IMPORTED_LOCATION "${SWRESAMPLE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SWRESAMPLE_INCLUDE_DIR}"
    )
    add_library(FFmpeg::swscale UNKNOWN IMPORTED)
    set_target_properties(FFmpeg::swscale PROPERTIES
        IMPORTED_LOCATION "${SWSCALE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SWSCALE_INCLUDE_DIR}"
    )

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
