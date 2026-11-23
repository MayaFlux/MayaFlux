#pragma once

namespace MayaFlux::Kakshya {

/**
 * @struct FaceList
 * @brief The polygons of a mesh: one size per face and the vertex of every corner.
 *
 * Corners are numbered consecutively, face after face, so the corners of face f follow those
 * of face f - 1. A face has at least three corners, and a vertex may be named by any number
 * of corners. MeshData keeps one when a mesh came from polygons, and Kinesis::Incidence
 * checks and indexes it.
 */
struct FaceList {
    std::vector<uint32_t> sizes;
    std::vector<uint32_t> corners;
};

} // namespace MayaFlux::Kakshya
