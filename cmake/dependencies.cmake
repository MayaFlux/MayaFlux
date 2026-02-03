if(WIN32)
    message(STATUS "=== Windows Dependency Detection ===")

    find_package(Vulkan REQUIRED)
    find_package(magic_enum REQUIRED)

    find_package(eigen3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(RtAudio CONFIG REQUIRED)
    find_package(hidapi CONFIG REQUIRED)
    find_package(rtmidi CONFIG REQUIRED)

    find_package(LLVM CONFIG REQUIRED HINTS "$ENV{LLVM_DIR}")
    find_package(Clang CONFIG REQUIRED HINTS "$ENV{Clang_DIR}")

    foreach(component avcodec avformat avutil swresample swscale)
        string(TOUPPER ${component} comp_upper)
        add_library(FFmpeg::${component} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${component} PROPERTIES
            IMPORTED_LOCATION "$ENV{FFMPEG_ROOT}/lib/${component}.lib"
            INTERFACE_INCLUDE_DIRECTORIES "$ENV{FFMPEG_ROOT}/include")
    endforeach()
else()
    message(STATUS "=== UNIX Dependency Detection ===")

    find_package(PkgConfig REQUIRED)

    if(APPLE)
        find_package(oneDPL)
        find_package(fmt REQUIRED)
        find_package(Vulkan REQUIRED)
        pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi)
    else()
        pkg_check_modules(Vulkan REQUIRED IMPORTED_TARGET vulkan)
        pkg_search_module(HIDAPI REQUIRED IMPORTED_TARGET
            hidapi-hidraw hidapi-libusb hidapi)
    endif()

    find_package(TBB REQUIRED)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED)
    find_package(glm REQUIRED)

    pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)
    pkg_check_modules(Eigen REQUIRED IMPORTED_TARGET eigen3)
    pkg_check_modules(magic_enum REQUIRED IMPORTED_TARGET magic_enum)
    pkg_check_modules(Glfw REQUIRED IMPORTED_TARGET glfw3>=3.4)
    pkg_check_modules(RtMidi REQUIRED IMPORTED_TARGET rtmidi)

    pkg_check_modules(LIBAVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(LIBAVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(LIBAVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(LIBSWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
    pkg_check_modules(LIBSWSCALE REQUIRED IMPORTED_TARGET libswscale)
endif()
