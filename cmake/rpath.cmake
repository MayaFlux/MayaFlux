if(NOT MAYAFLUX_PORTABLE)
    set(CMAKE_SKIP_RPATH ON)
    set(CMAKE_SKIP_INSTALL_RPATH ON)
else()
    # Portable builds - use RPATH
    set(CMAKE_SKIP_RPATH OFF)
    set(CMAKE_SKIP_INSTALL_RPATH OFF)

    # Platform-specific RPATH
    if(APPLE)
        set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../lib")
    else()
        set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib")
    endif()

    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()
