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
    find_package(asio CONFIG REQUIRED)
    find_package(assimp CONFIG REQUIRED)
    find_package(freetype CONFIG REQUIRED)
    find_package(utf8proc CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)

    find_package(LLVM CONFIG REQUIRED HINTS "$ENV{LLVM_DIR}")
    find_package(Clang CONFIG REQUIRED HINTS "$ENV{Clang_DIR}")

    foreach(component avcodec avformat avutil swresample swscale avdevice)
        string(TOUPPER ${component} comp_upper)
        add_library(FFmpeg::${component} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${component} PROPERTIES
            IMPORTED_LOCATION "$ENV{FFMPEG_ROOT}/lib/${component}.lib"
            INTERFACE_INCLUDE_DIRECTORIES "$ENV{FFMPEG_ROOT}/include")
    endforeach()

    set(MAYAFLUX_EIGEN_INCLUDE ${eigen3_INCLUDE_DIRS})
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
        pkg_check_modules(Fontconfig REQUIRED IMPORTED_TARGET fontconfig)
        pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

        if(PIPEWIRE_FOUND)
            add_compile_definitions(PIPEWIRE_BACKEND)
        else()
            add_compile_definitions(RTAUDIO_BACKEND)
        endif()
    endif()

    find_package(TBB REQUIRED)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED)
    find_package(glm REQUIRED)
    find_package(Freetype REQUIRED)

    pkg_check_modules(RtAudio REQUIRED IMPORTED_TARGET rtaudio)
    pkg_check_modules(Eigen REQUIRED IMPORTED_TARGET eigen3)
    pkg_check_modules(magic_enum REQUIRED IMPORTED_TARGET magic_enum)
    pkg_check_modules(Glfw REQUIRED IMPORTED_TARGET glfw3>=3.4)
    pkg_check_modules(RtMidi REQUIRED IMPORTED_TARGET rtmidi)
    pkg_check_modules(Asio REQUIRED IMPORTED_TARGET asio)
    pkg_check_modules(Assimp REQUIRED IMPORTED_TARGET assimp)

    pkg_check_modules(LIBAVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(LIBAVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(LIBAVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(LIBSWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
    pkg_check_modules(LIBSWSCALE REQUIRED IMPORTED_TARGET libswscale)
    pkg_check_modules(LIBAVDEVICE REQUIRED IMPORTED_TARGET libavdevice)
    pkg_check_modules(UTF8PROC REQUIRED IMPORTED_TARGET libutf8proc)
    pkg_check_modules(nlohmann_json REQUIRED IMPORTED_TARGET nlohmann_json)

    set(MAYAFLUX_EIGEN_INCLUDE ${Eigen_INCLUDE_DIRS})
    list(GET FREETYPE_INCLUDE_DIRS 0 MAYAFLUX_FREETYPE_INCLUDE)
endif()

add_library(miniz STATIC ${CMAKE_SOURCE_DIR}/third_party/tinyexr/miniz.c)
set_target_properties(miniz PROPERTIES LINKER_LANGUAGE C)
target_include_directories(miniz PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/tinyexr>
    $<INSTALL_INTERFACE:include/MayaFlux/thirdparty/tinyexr>
)

add_library(tinyexr INTERFACE)
target_include_directories(tinyexr INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/tinyexr>
    $<INSTALL_INTERFACE:include/MayaFlux/thirdparty/tinyexr>
)
target_link_libraries(tinyexr INTERFACE miniz)
