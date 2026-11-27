include_guard(GLOBAL)

file(GLOB_RECURSE IO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

if(APPLE)
    file(GLOB_RECURSE IO_MM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/*.mm"
    )
    list(APPEND IO_SOURCES ${IO_MM_SOURCES})
endif()

file(GLOB_RECURSE IO_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

list(APPEND MAYAFLUX_ALL_SOURCES ${IO_SOURCES})
list(APPEND MAYAFLUX_ALL_HEADERS ${IO_HEADERS})

list(APPEND MAYAFLUX_HEAVY_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Detail/IOTransitions.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IOManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Image/EXRWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Image/ImageExport.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Image/ImageReader.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Image/STBImageWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Model/AlembicModelWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Model/AssimpModelWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Model/ModelExport.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Model/ModelReader.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Spatial/SpatialExport.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Spatial/SpatialTransfer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Video/VideoFileWriter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Volume/VolumeTransfer.cpp"
)
