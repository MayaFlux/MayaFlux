install(TARGETS MayaFluxLib DESTINATION lib)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/MayaFlux/
        DESTINATION include/MayaFlux
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mayaflux.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc
    @ONLY
)

if(UNIX)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mayaflux.pc DESTINATION lib/pkgconfig)
endif()

if(LLVM_FOUND)
    install(TARGETS Lila
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/ClangInterpreter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/Lila.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Lila/Server.hpp
        DESTINATION include/Lila
    )

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/pch.h
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h
        DESTINATION share/lila/runtime
    )

    message(STATUS "Lila will be installed to ${CMAKE_INSTALL_PREFIX}")
endif()
