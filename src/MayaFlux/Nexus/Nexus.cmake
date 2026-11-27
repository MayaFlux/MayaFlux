include_guard(GLOBAL)

file(GLOB_RECURSE NEXUS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

file(GLOB_RECURSE NEXUS_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

list(APPEND MAYAFLUX_ALL_SOURCES ${NEXUS_SOURCES})
list(APPEND MAYAFLUX_ALL_HEADERS ${NEXUS_HEADERS})

list(APPEND MAYAFLUX_HEAVY_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Pheme/Attachment.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Pheme/InfluenceList.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Pheme/Sinks.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Pheme/Survey.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Principals/Agent.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Principals/Emitter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Principals/Locus.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/State/Decoder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/State/Encoder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/State/Orchestrator.cpp"
)
