include_guard(GLOBAL)

file(GLOB_RECURSE KRIYA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

file(GLOB_RECURSE KRIYA_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

list(APPEND MAYAFLUX_ALL_SOURCES ${KRIYA_SOURCES})
list(APPEND MAYAFLUX_ALL_HEADERS ${KRIYA_HEADERS})

list(APPEND MAYAFLUX_HEAVY_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/BufferOperation.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BufferPipeline.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Capture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Chimera.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/MeshReflow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/MotionReflow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PipelineHelpers/BufferPipelineCapture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PipelineHelpers/BufferPipelineOperations.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PipelineHelpers/PipelineGraphicsData.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Realization/MeshAssemblyRealization.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SamplingPipeline.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SpatialReflow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Timers.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VolumeReflow.cpp"
)
