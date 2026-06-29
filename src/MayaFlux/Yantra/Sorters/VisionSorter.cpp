#include "VisionSorter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra {

// =============================================================================
// key_for_box
// =============================================================================

float VisionSorter::key_for_box(const Kinesis::Vision::BoundingBox& box) const
{
    switch (m_key) {
    case VisionSortKey::AREA:
        return box.w * box.h;
    case VisionSortKey::PERIMETER:
        return 2.0F * (box.w + box.h);
    case VisionSortKey::CENTROID_X:
        return box.x + box.w * 0.5F;
    case VisionSortKey::CENTROID_Y:
        return box.y + box.h * 0.5F;
    case VisionSortKey::WIDTH:
        return box.w;
    case VisionSortKey::HEIGHT:
        return box.h;
    }
    return 0.0F;
}

// =============================================================================
// key_for_contour
// =============================================================================

float VisionSorter::key_for_contour(const Kinesis::Vision::Contour& contour) const
{
    switch (m_key) {
    case VisionSortKey::AREA:
        return contour.area;
    case VisionSortKey::PERIMETER:
        return contour.perimeter;
    case VisionSortKey::WIDTH:
    case VisionSortKey::HEIGHT:
    case VisionSortKey::CENTROID_X:
    case VisionSortKey::CENTROID_Y: {
        if (contour.points.empty())
            return 0.0F;
        glm::vec2 mn = contour.points[0];
        glm::vec2 mx = contour.points[0];
        for (const auto& p : contour.points) {
            if (p.x < mn.x)
                mn.x = p.x;
            if (p.y < mn.y)
                mn.y = p.y;
            if (p.x > mx.x)
                mx.x = p.x;
            if (p.y > mx.y)
                mx.y = p.y;
        }
        if (m_key == VisionSortKey::WIDTH)
            return mx.x - mn.x;
        if (m_key == VisionSortKey::HEIGHT)
            return mx.y - mn.y;
        if (m_key == VisionSortKey::CENTROID_X)
            return (mn.x + mx.x) * 0.5F;
        return (mn.y + mx.y) * 0.5F;
    }
    }
    return 0.0F;
}

// =============================================================================
// sort_implementation
// =============================================================================

VisionSorter::output_type VisionSorter::sort_implementation(const input_type& input)
{
    const auto analysis_it = input.metadata.find("vision_analysis");
    if (analysis_it == input.metadata.end() || !analysis_it->second.has_value()) {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "VisionSorter: no vision_analysis in input metadata");
    }

    auto analysis = safe_any_cast_or_throw<VisionAnalysis>(analysis_it->second);

    const bool descending = (this->get_direction() == SortingDirection::DESCENDING);

    auto sort_indices = [&](size_t count) -> std::vector<size_t> {
        std::vector<size_t> idx(count);
        std::iota(idx.begin(), idx.end(), 0);
        std::ranges::stable_sort(idx,
            [&](size_t a, size_t b) {
                const float ka = m_key_fn ? m_key_fn(a, analysis) : 0.0F;
                const float kb = m_key_fn ? m_key_fn(b, analysis) : 0.0F;
                return descending ? ka > kb : ka < kb;
            });
        return idx;
    };

    if (!analysis.frame.boxes.empty()) {
        const size_t n = analysis.frame.boxes.size();
        std::vector<size_t> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        std::ranges::stable_sort(idx,
            [&](size_t a, size_t b) {
                const float ka = m_key_fn
                    ? m_key_fn(a, analysis)
                    : key_for_box(analysis.frame.boxes[a]);
                const float kb = m_key_fn
                    ? m_key_fn(b, analysis)
                    : key_for_box(analysis.frame.boxes[b]);
                return descending ? ka > kb : ka < kb;
            });

        std::vector<Kinesis::Vision::BoundingBox> sorted(n);
        for (size_t i = 0; i < n; ++i)
            sorted[i] = analysis.frame.boxes[idx[i]];
        analysis.frame.boxes = std::move(sorted);
    }

    if (!analysis.frame.contours.empty()) {
        const size_t n = analysis.frame.contours.size();
        std::vector<size_t> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        std::ranges::stable_sort(idx,
            [&](size_t a, size_t b) {
                const float ka = m_key_fn
                    ? m_key_fn(a, analysis)
                    : key_for_contour(analysis.frame.contours[a]);
                const float kb = m_key_fn
                    ? m_key_fn(b, analysis)
                    : key_for_contour(analysis.frame.contours[b]);
                return descending ? ka > kb : ka < kb;
            });

        std::vector<Kinesis::Vision::Contour> sorted(n);
        for (size_t i = 0; i < n; ++i)
            sorted[i] = analysis.frame.contours[idx[i]];
        analysis.frame.contours = std::move(sorted);
    }

    output_type out { input.data };
    out.metadata = input.metadata;
    out.metadata["vision_analysis"] = std::move(analysis);
    return out;
}

} // namespace MayaFlux::Yantra
