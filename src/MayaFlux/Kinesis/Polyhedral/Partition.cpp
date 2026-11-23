#include "Partition.hpp"

namespace MayaFlux::Kinesis {

FacePartition partition_faces(std::span<const uint32_t> face_group)
{
    FacePartition result;

    result.group_labels.assign(face_group.begin(), face_group.end());
    std::ranges::sort(result.group_labels);
    const auto tail = std::ranges::unique(result.group_labels);
    result.group_labels.erase(tail.begin(), tail.end());

    const size_t groups = result.group_labels.size();
    std::vector<uint32_t> rank(face_group.size());
    detail::for_each_indexed(std::execution::par_unseq, std::span<uint32_t>(rank),
        [&](size_t face, uint32_t& out) {
            const auto it = std::ranges::lower_bound(result.group_labels, face_group[face]);
            out = static_cast<uint32_t>(it - result.group_labels.begin());
        });

    result.group_offsets.assign(groups + 1, 0U);
    for (const uint32_t r : rank) {
        ++result.group_offsets[static_cast<size_t>(r) + 1];
    }
    std::partial_sum(result.group_offsets.begin(), result.group_offsets.end(), result.group_offsets.begin());

    result.face_order.resize(face_group.size());
    std::vector<uint32_t> cursor(result.group_offsets.begin(), result.group_offsets.end() - 1);
    for (uint32_t face = 0; face < face_group.size(); ++face) {
        result.face_order[cursor[rank[face]]++] = face;
    }
    return result;
}

std::optional<FacePartition> partition_faces(
    size_t face_count, std::span<const std::span<const uint32_t>> groups)
{
    constexpr uint64_t limit = std::numeric_limits<uint32_t>::max();
    if (face_count >= limit || groups.size() >= limit) {
        return std::nullopt;
    }

    std::vector<uint8_t> named(face_count, 0);
    uint64_t total = 0;
    for (const auto& group : groups) {
        total += group.size();
        if (total > limit) {
            return std::nullopt;
        }
        for (const uint32_t face : group) {
            if (face >= face_count) {
                return std::nullopt;
            }
            named[face] = 1;
        }
    }

    FacePartition result;
    result.face_order.reserve(total + face_count);
    result.group_offsets.reserve(groups.size() + 2);
    result.group_offsets.push_back(0);
    for (size_t g = 0; g < groups.size(); ++g) {
        result.face_order.insert(result.face_order.end(), groups[g].begin(), groups[g].end());
        result.group_offsets.push_back(static_cast<uint32_t>(result.face_order.size()));
        result.group_labels.push_back(static_cast<uint32_t>(g));
    }

    const size_t before = result.face_order.size();
    for (uint32_t face = 0; face < face_count; ++face) {
        if (named[face] == 0) {
            result.face_order.push_back(face);
        }
    }
    if (result.face_order.size() > limit) {
        return std::nullopt;
    }
    if (result.face_order.size() > before) {
        result.group_offsets.push_back(static_cast<uint32_t>(result.face_order.size()));
        result.group_labels.push_back(k_no_group);
    }
    return result;
}

std::optional<FaceList> reorder_faces(const Incidence& incidence, std::span<const uint32_t> face_order)
{
    constexpr uint64_t limit = std::numeric_limits<uint32_t>::max();
    if (face_order.size() >= limit) {
        return std::nullopt;
    }

    FaceList result;
    result.sizes.resize(face_order.size());
    std::vector<uint32_t> starts(face_order.size() + 1, 0U);

    uint64_t running = 0;
    for (size_t k = 0; k < face_order.size(); ++k) {
        if (face_order[k] >= incidence.face_count()) {
            return std::nullopt;
        }
        const uint32_t size = incidence.sizes()[face_order[k]];
        result.sizes[k] = size;
        starts[k] = static_cast<uint32_t>(running);
        running += size;
        if (running > limit) {
            return std::nullopt;
        }
    }
    starts.back() = static_cast<uint32_t>(running);

    result.corners.resize(running);
    detail::for_each_indexed(std::execution::par_unseq, face_order,
        [&](size_t k, const uint32_t& face) {
            const auto source = incidence.face(face);
            std::ranges::copy(source, result.corners.begin() + starts[k]);
        });
    return result;
}

} // namespace MayaFlux::Kinesis
