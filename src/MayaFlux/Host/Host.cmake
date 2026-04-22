include_guard(GLOBAL)

if(MAYAFLUX_ENABLE_LILA)
    list(APPEND MAYAFLUX_ALL_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/Lila/Attach.cpp
    )
    list(APPEND MAYAFLUX_ALL_HEADERS
        ${CMAKE_CURRENT_LIST_DIR}/Lila/Attach.hpp
    )
endif()
