#include "VisionExtractor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kinesis/Vision/Contours.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"
#include "MayaFlux/Yantra/Analyzers/VisionAnalyzer.hpp"

namespace MayaFlux::Yantra {

// =============================================================================
// infer_format
// =============================================================================

Portal::Graphics::ImageFormat VisionExtractor::infer_format(
    const Kakshya::SignalSourceContainer& container)
{
    const auto& t = container.value_element_type();
    if (t == typeid(float))
        return Portal::Graphics::ImageFormat::RGBA32F;
    if (t == typeid(uint16_t))
        return Portal::Graphics::ImageFormat::RGBA16;

    return Portal::Graphics::ImageFormat::RGBA8;
}

// =============================================================================
// extract_implementation
// =============================================================================

VisionExtractor::output_type VisionExtractor::extract_implementation(
    const input_type& input)
{
    if (!input.data) {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "VisionExtractor: null container in input");
    }

    const auto analysis_it = input.metadata.find("vision_analysis");
    if (analysis_it == input.metadata.end() || !analysis_it->second.has_value()) {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "VisionExtractor: no vision_analysis in input metadata");
    }

    const auto& analysis = safe_any_cast_or_throw<VisionAnalysis>(analysis_it->second);

    const auto& container = *input.data;
    const auto dims = container.get_dimensions();

    uint32_t w = 0;
    uint32_t h = 0;
    for (const auto& dim : dims) {
        if (dim.role == Kakshya::DataDimension::Role::SPATIAL_X) {
            w = static_cast<uint32_t>(dim.size);
        } else if (dim.role == Kakshya::DataDimension::Role::SPATIAL_Y) {
            h = static_cast<uint32_t>(dim.size);
        }
    }

    if (w == 0 || h == 0) {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "VisionExtractor: container has no resolvable spatial dimensions");
    }

    // -------------------------------------------------------------------------
    // Build the pixel-space Region from the detection result
    // -------------------------------------------------------------------------

    Kakshya::Region region;
    const Kinesis::Vision::Contour* contour_ptr = nullptr;

    switch (m_mode) {
    case VisionExtractionMode::BBOX: {
        if (m_index >= analysis.frame.boxes.size()) {
            error<std::out_of_range>(
                Journal::Component::Yantra,
                Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "VisionExtractor: bbox index {} out of range ({})",
                m_index, analysis.frame.boxes.size());
        }
        const auto& box = analysis.frame.boxes[m_index];
        region = Kakshya::normalised_rect_to_region(box.x, box.y, box.w, box.h, w, h);
        break;
    }
    case VisionExtractionMode::CONTOUR_TIGHT:
    case VisionExtractionMode::CONTOUR_MASKED: {
        if (m_index >= analysis.frame.contours.size()) {
            error<std::out_of_range>(
                Journal::Component::Yantra,
                Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "VisionExtractor: contour index {} out of range ({})",
                m_index, analysis.frame.contours.size());
        }
        contour_ptr = &analysis.frame.contours[m_index];
        region = Kakshya::normalised_points_tight_region(
            std::span<const glm::vec2>(contour_ptr->points),
            w, h);
        break;
    }
    }

    // -------------------------------------------------------------------------
    // Crop via the container's existing regional accessor
    // -------------------------------------------------------------------------

    const auto variants = input.data->get_region_data(region);
    if (variants.empty()) {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "VisionExtractor: get_region_data returned empty result");
    }

    const auto crop_w = static_cast<uint32_t>(region.end_coordinates[1] - region.start_coordinates[1] + 1);
    const auto crop_h = static_cast<uint32_t>(region.end_coordinates[0] - region.start_coordinates[0] + 1);

    // -------------------------------------------------------------------------
    // Materialise into a TextureContainer
    // -------------------------------------------------------------------------

    const auto fmt = infer_format(container);
    auto result_tc = std::make_shared<Kakshya::TextureContainer>(crop_w, crop_h, fmt);
    result_tc->set_region_data(
        Kakshya::Region(std::vector { uint64_t { 0 }, uint64_t { 0 } },
            std::vector { static_cast<uint64_t>(crop_h) - 1, static_cast<uint64_t>(crop_w) - 1 }),
        variants);

    // -------------------------------------------------------------------------
    // Contour polygon mask (CONTOUR_MASKED only)
    // -------------------------------------------------------------------------

    if (m_mode == VisionExtractionMode::CONTOUR_MASKED && contour_ptr) {
        const auto float_span = result_tc->as_normalised_float(0);
        if (!float_span.empty()) {
            const auto channels = static_cast<uint32_t>(
                float_span.size() / (static_cast<size_t>(crop_w) * crop_h));

            std::vector<float> mutable_pixels(float_span.begin(), float_span.end());

            const float origin_x = static_cast<float>(region.start_coordinates[1]) / static_cast<float>(w);
            const float origin_y = static_cast<float>(region.start_coordinates[0]) / static_cast<float>(h);
            const float scale_x = 1.0F / static_cast<float>(w);
            const float scale_y = 1.0F / static_cast<float>(h);

            Kinesis::Vision::apply_contour_mask(
                std::span<float>(mutable_pixels),
                crop_w, crop_h,
                channels,
                *contour_ptr,
                origin_x, origin_y,
                scale_x, scale_y);

            result_tc->set_region_data(
                Kakshya::Region(std::vector { uint64_t { 0 }, uint64_t { 0 } },
                    std::vector { static_cast<uint64_t>(crop_h) - 1, static_cast<uint64_t>(crop_w) - 1 }),
                { Kakshya::DataVariant(std::move(mutable_pixels)) });
        }
    }

    output_type out { std::static_pointer_cast<Kakshya::SignalSourceContainer>(result_tc) };
    out.metadata = input.metadata;
    out.metadata["vision_extractor_mode"] = static_cast<int>(m_mode);
    out.metadata["vision_extractor_index"] = m_index;
    out.metadata["crop_w"] = crop_w;
    out.metadata["crop_h"] = crop_h;
    return out;
}

} // namespace MayaFlux::Yantra
