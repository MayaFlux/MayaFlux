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
