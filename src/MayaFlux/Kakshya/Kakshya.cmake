include_guard(GLOBAL)

file(GLOB_RECURSE KAKSHYA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

file(GLOB_RECURSE KAKSHYA_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

list(APPEND MAYAFLUX_ALL_SOURCES ${KAKSHYA_SOURCES})
list(APPEND MAYAFLUX_ALL_HEADERS ${KAKSHYA_HEADERS})

list(APPEND MAYAFLUX_HEAVY_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Processors/VisionProcessor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Processors/WindowAccessProcessor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/DynamicVideoStream.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/TextureCollection.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/TextureContainer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/VideoStreamContainer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/WindowContainer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Utils/PixelStorage.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Utils/SurfaceUtils.cpp"
)
