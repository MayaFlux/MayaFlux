if(WIN32)
    # message(STATUS "Fetching GoogleTest via FetchContent...")
    # FetchContent_Declare(
    #     googletest
    #     GIT_REPOSITORY https://github.com/google/googletest.git
    #     GIT_TAG v1.14.0
    #     GIT_SHALLOW TRUE
    # )
    # set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    # set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    # FetchContent_GetProperties(googletest)
    # if(NOT googletest_POPULATED)
    #     FetchContent_Populate(googletest)
    # endif()

    #
    # if(EXISTS "${googletest_SOURCE_DIR}")
    #     add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
    #     set_target_properties(gtest gtest_main PROPERTIES
    #         MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    #     )
    #     target_compile_options(gtest PRIVATE
    #         /MDd$<$<CONFIG:Release>:/MT>
    #     )
    #     target_compile_options(gtest_main PRIVATE
    #         /MDd$<$<CONFIG:Release>:/MT>
    #     )
    # endif()

    # if(EXISTS "${magic_enum_SOURCE_DIR}")
    #     add_subdirectory(${magic_enum_SOURCE_DIR} ${magic_enum_BINARY_DIR} EXCLUDE_FROM_ALL)
    # endif()

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
