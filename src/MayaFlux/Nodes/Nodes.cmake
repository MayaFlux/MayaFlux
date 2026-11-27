include_guard(GLOBAL)

file(GLOB_RECURSE NODES_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

file(GLOB_RECURSE NODES_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

list(APPEND MAYAFLUX_ALL_SOURCES ${NODES_SOURCES})
list(APPEND MAYAFLUX_ALL_HEADERS ${NODES_HEADERS})

list(APPEND MAYAFLUX_HEAVY_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Graphics/GeometryReadbackNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Graphics/GlyphGeometryNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Graphics/GpuComputeNode.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Network/MeshNetwork.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Network/Operators/InstanceFieldOperator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Network/Operators/MeshFieldOperator.cpp"
)
