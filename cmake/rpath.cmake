if(NOT MAYAFLUX_PORTABLE)
    set(CMAKE_SKIP_RPATH ON)
    set(CMAKE_SKIP_INSTALL_RPATH ON)
else()
    set(CMAKE_SKIP_RPATH OFF)
    set(CMAKE_SKIP_INSTALL_RPATH OFF)

    if(APPLE)
        set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../lib")
        set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

    elseif(UNIX AND NOT APPLE)

        set(CMAKE_SKIP_BUILD_RPATH OFF)
        set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

        set(CMAKE_BUILD_RPATH
            "${CMAKE_BINARY_DIR}/src/MayaFlux"
            "${CMAKE_BINARY_DIR}/src/Lila"
        )
        set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib")

    elseif(WIN32)

        set(CMAKE_SKIP_BUILD_RPATH OFF)
        set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
        set(CMAKE_BUILD_RPATH
            "${CMAKE_BINARY_DIR}/src/MayaFlux"
            "${CMAKE_BINARY_DIR}/src/Lila"
        )
    endif()
endif()
