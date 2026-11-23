#pragma once

#include "Incidence.hpp"

#include "MayaFlux/Kakshya/NDData/FaceList.hpp"

namespace MayaFlux::Kinesis {

/**
 * @brief The label given to faces that belong to no group.
 */
inline constexpr uint32_t k_no_group = std::numeric_limits<uint32_t>::max();

/**
 * @struct FacePartition
 * @brief Faces gathered into groups, each group a contiguous range of an order.
 *
 * Group g holds face_order[group_offsets[g]] up to face_order[group_offsets[g + 1]].
 */
struct FacePartition {
    std::vector<uint32_t> face_order; ///< A face index at each position.
    std::vector<uint32_t> group_offsets; ///< One more entry than there are groups.
    std::vector<uint32_t> group_labels; ///< The label of each group.
};

/**
 * @brief Gather faces that share a label.
 *
 * Every face has one label and lands in exactly one group. Groups come in ascending
 * label order, and faces keep their order inside a group.
 */
[[nodiscard]] MAYAFLUX_API FacePartition partition_faces(std::span<const uint32_t> face_group);

/**
 * @brief Gather faces by membership lists that may overlap.
 *
 * Group g lists its faces in the order given, and a face named by several groups appears
 * in each. Faces named by none follow as one last group labelled k_no_group, when there are
 * any. Labels of the listed groups are their positions in @p groups.
 *
 * @return nullopt when a member is not below @p face_count, or the order would not fit
 * in 32 bits.
 */
[[nodiscard]] MAYAFLUX_API std::optional<FacePartition> partition_faces(
    size_t face_count, std::span<const std::span<const uint32_t>> groups);

/**
 * @brief The faces of @p incidence laid out in the order of @p face_order.
 *
 * A face may appear more than once and need not appear at all.
 *
 * @return nullopt when an entry is not a face of the incidence, or the corners would not
 * fit in 32 bits.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Kakshya::FaceList> reorder_faces(
    const Incidence& incidence, std::span<const uint32_t> face_order);

} // namespace MayaFlux::Kinesis
