include(GNUInstallDirs)

if(WIN32)
    message(STATUS "=== Windows Dependency Detection ===")

    find_package(Vulkan REQUIRED)

    find_package(eigen3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(hidapi CONFIG REQUIRED)
    find_package(asio CONFIG REQUIRED)
    find_package(assimp CONFIG REQUIRED)
    find_package(freetype CONFIG REQUIRED)
    find_package(utf8proc CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)

    cmake_path(CONVERT "$ENV{LLVM_DEV_DIR}" TO_CMAKE_PATH_LIST _llvm_hint NORMALIZE)
    cmake_path(CONVERT "$ENV{Clang_DEV_DIR}" TO_CMAKE_PATH_LIST _clang_hint NORMALIZE)
    find_package(LLVM CONFIG REQUIRED HINTS "${_llvm_hint}")
    find_package(Clang CONFIG REQUIRED HINTS "${_clang_hint}")

    cmake_path(CONVERT "$ENV{FFMPEG_ROOT}" TO_CMAKE_PATH_LIST _ffmpeg_root NORMALIZE)
    foreach(component avcodec avformat avutil swresample swscale avdevice)
        add_library(FFmpeg::${component} UNKNOWN IMPORTED)
        set_target_properties(FFmpeg::${component} PROPERTIES
            IMPORTED_LOCATION "${_ffmpeg_root}/lib/${component}.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${_ffmpeg_root}/include")
    endforeach()

    set(MAYAFLUX_EIGEN_INCLUDE ${eigen3_INCLUDE_DIRS})
else()
    message(STATUS "=== UNIX Dependency Detection ===")

    find_package(PkgConfig REQUIRED)

    if(APPLE)
        find_package(oneDPL)
        find_package(Vulkan REQUIRED)
        pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi)
        pkg_check_modules(Glfw REQUIRED IMPORTED_TARGET glfw3>=3.4)
    else()
        pkg_check_modules(Vulkan REQUIRED IMPORTED_TARGET vulkan)
        pkg_search_module(HIDAPI REQUIRED IMPORTED_TARGET
            hidapi-hidraw hidapi-libusb hidapi)
        pkg_check_modules(Fontconfig REQUIRED IMPORTED_TARGET fontconfig)

        add_compile_definitions(PIPEWIRE_BACKEND)
        pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

        pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET
                        wayland-client)

        pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
        pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)

        find_program(WaylandScanner_EXECUTABLE NAMES wayland-scanner REQUIRED)
        pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

        pkg_check_modules(DBUS REQUIRED dbus-1)
    endif()

    find_package(TBB REQUIRED)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED)
    find_package(glm REQUIRED)
    find_package(Freetype REQUIRED)

    pkg_check_modules(Eigen REQUIRED IMPORTED_TARGET eigen3)
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
set_target_properties(miniz PROPERTIES
    LINKER_LANGUAGE C
    POSITION_INDEPENDENT_CODE ON
)
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

add_library(magic_enum INTERFACE)
target_include_directories(magic_enum INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/magic_enum>
    $<INSTALL_INTERFACE:include/MayaFlux/thirdparty/magic_enum>
)
