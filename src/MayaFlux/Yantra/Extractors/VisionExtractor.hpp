#pragma once

#include "UniversalExtractor.hpp"

#include "MayaFlux/Yantra/Analyzers/VisionAnalyzer.hpp"
#include "MayaFlux/Yantra/Executors/TextureExecutionContext.hpp"

#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kinesis/Vision/Contours.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum VisionExtractionMode
 * @brief Selects the spatial mask used to crop pixels from a container.
 */
enum class VisionExtractionMode : uint8_t {
    BBOX, ///< Crop the axis-aligned rectangle of a BoundingBox.
    CONTOUR_TIGHT, ///< Crop the tight bounding rect of a Contour; no polygon mask.
    CONTOUR_MASKED, ///< Crop the tight bounding rect of a Contour; zero pixels outside polygon.
};

/**
 * @class VisionExtractor
 * @brief Extracts a pixel sub-region guided by a VisionAnalysis in input metadata.
 *
 * Accepts any pixel-bearing InputType: vector<DataVariant>, shared_ptr<SignalSourceContainer>,
 * Region, or RegionGroup. extract_structured_native handles all unwrapping.
 * VisionAnalysis must be present in input.metadata["vision_analysis"].
 *
 * CPU path: reads pixel data via extract_structured_native, converts normalised
 * bbox or contour rect to a pixel Region via CoordUtils, copies the sub-region,
 * and optionally applies apply_contour_mask. Output data is vector<DataVariant>
 * carrying the cropped float pixels with IMAGE_2D dimensions set.
 *
 * GPU path: attach a TextureExecutionContext configured with vision_crop.comp.
 * Before calling apply_operation, use compute_normalised_rect() to get the crop
 * rect, then call set_output_dimensions() and set_push_constants() on the context.
 * apply_operation_internal routes to the backend automatically.
 *
 * @tparam InputType  Any ComputeData type carrying pixel data.
 *                    Defaults to shared_ptr<SignalSourceContainer>.
 * @tparam OutputType Output pixel data type. Defaults to vector<DataVariant>.
 */
template <ComputeData InputType = std::shared_ptr<Kakshya::SignalSourceContainer>,
    ComputeData OutputType = std::vector<Kakshya::DataVariant>>
class VisionExtractor
    : public UniversalExtractor<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    /**
     * @brief Construct with extraction mode and target index.
     * @param mode  Crop strategy to apply.
     * @param index Zero-based index into boxes or contours in VisionAnalysis.
     */
    explicit VisionExtractor(
        VisionExtractionMode mode = VisionExtractionMode::BBOX,
        uint32_t index = 0)
        : m_mode(mode)
        , m_index(index)
    {
    }

    void set_mode(VisionExtractionMode mode) { m_mode = mode; }
    void set_index(uint32_t index) { m_index = index; }

    [[nodiscard]] VisionExtractionMode get_mode() const { return m_mode; }
    [[nodiscard]] uint32_t get_index() const { return m_index; }

    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::REGION_BASED;
    }

    [[nodiscard]] std::string get_extractor_name() const override
    {
        return "VisionExtractor";
    }

    [[nodiscard]] std::vector<std::string> get_available_methods() const override
    {
        return { "bbox", "contour_tight", "contour_masked" };
    }

    /**
     * @brief Compute the normalised crop rectangle for a given VisionAnalysis.
     *
     * Returns { nx, ny, nw, nh } in image space [0,1]. Used by callers
     * to configure a TextureExecutionContext before GPU dispatch:
     * @code
     * auto rect = extractor->compute_normalised_rect(analysis);
     * ctx->set_output_dimensions(crop_w, crop_h);
     * ctx->set_push_constants(CropPC { rect[0], rect[1], rect[2], rect[3], crop_w, crop_h });
     * extractor->apply_operation(datum);
     * @endcode
     *
     * @throws std::out_of_range if index exceeds the relevant detection collection.
     */
    [[nodiscard]] std::array<float, 4> compute_normalised_rect(
        const VisionAnalysis& analysis) const
    {
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
            const auto& b = analysis.frame.boxes[m_index];
            return { b.x, b.y, b.w, b.h };
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
            const auto& pts = analysis.frame.contours[m_index].points;
            float min_x = pts[0].x, max_x = pts[0].x;
            float min_y = pts[0].y, max_y = pts[0].y;
            for (const auto& p : pts) {
                if (p.x < min_x)
                    min_x = p.x;
                if (p.x > max_x)
                    max_x = p.x;
                if (p.y < min_y)
                    min_y = p.y;
                if (p.y > max_y)
                    max_y = p.y;
            }
            return { min_x, min_y, max_x - min_x, max_y - min_y };
        }
        }
        return { 0.F, 0.F, 1.F, 1.F };
    }

protected:
    output_type apply_operation_internal(
        const input_type& input, const ExecutionContext& context) override
    {
        if (this->m_gpu_backend && this->m_gpu_backend->ensure_gpu_ready()) {
            const auto analysis_it = input.metadata.find("vision_analysis");
            if (analysis_it != input.metadata.end() && analysis_it->second.has_value()) {
                const auto& analysis = safe_any_cast_or_throw<VisionAnalysis>(
                    analysis_it->second);

                auto [nx, ny, nw, nh] = compute_normalised_rect(analysis);
                const auto crop_w = static_cast<uint32_t>(
                    std::max(1.0F, std::round(nw * static_cast<float>(analysis.w))));
                const auto crop_h = static_cast<uint32_t>(
                    std::max(1.0F, std::round(nh * static_cast<float>(analysis.h))));

                auto* ctx = dynamic_cast<TextureExecutionContext*>(
                    this->m_gpu_backend.get());
                if (ctx) {
                    ctx->set_output_dimensions(crop_w, crop_h);
                    ctx->set_push_constants(
                        CropPC { nx, ny, nw, nh, crop_w, crop_h });
                }
            }
        }

        return UniversalExtractor<InputType, OutputType>::apply_operation_internal(input, context);
    }

    output_type extract_implementation(const input_type& input) override
    {
        const auto analysis_it = input.metadata.find("vision_analysis");
        if (analysis_it == input.metadata.end() || !analysis_it->second.has_value()) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "VisionExtractor: no vision_analysis in input metadata");
        }

        const auto& analysis = safe_any_cast_or_throw<VisionAnalysis>(
            analysis_it->second);

        if (analysis.pixel_image.empty()) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "VisionExtractor: VisionAnalysis carries no pixel data");
        }

        const uint32_t w = analysis.w;
        const uint32_t h = analysis.h;
        const auto channels = static_cast<uint32_t>(
            analysis.pixel_image.size() / (static_cast<size_t>(w) * h));

        const auto [nx, ny, nw, nh] = compute_normalised_rect(analysis);

        Kakshya::Region region = Kakshya::normalised_rect_to_region(nx, ny, nw, nh, w, h);

        const auto crop_w = static_cast<uint32_t>(
            region.end_coordinates[1] - region.start_coordinates[1] + 1);
        const auto crop_h = static_cast<uint32_t>(
            region.end_coordinates[0] - region.start_coordinates[0] + 1);

        const uint32_t stride = w * channels;
        const uint32_t crop_stride = crop_w * channels;
        std::vector<float> cropped(static_cast<size_t>(crop_w) * crop_h * channels);

        const float* src = analysis.pixel_image.data();
        float* dst = cropped.data();

        const auto y0 = static_cast<uint32_t>(region.start_coordinates[0]);
        const auto x0 = static_cast<uint32_t>(region.start_coordinates[1]);

        for (uint32_t row = 0; row < crop_h; ++row) {
            std::memcpy(
                dst + static_cast<size_t>(row * crop_stride),
                src + (static_cast<size_t>(y0 + row) * stride) + static_cast<size_t>(x0 * channels),
                crop_stride * sizeof(float));
        }

        if (m_mode == VisionExtractionMode::CONTOUR_MASKED) {
            const auto& contour = analysis.frame.contours[m_index];
            const float origin_x = nx;
            const float origin_y = ny;
            const float scale_x = nw / static_cast<float>(crop_w);
            const float scale_y = nh / static_cast<float>(crop_h);
            Kinesis::Vision::apply_contour_mask(
                std::span<float>(cropped),
                crop_w, crop_h, channels,
                contour,
                origin_x, origin_y, scale_x, scale_y);
        }

        output_type out;
        if constexpr (std::is_same_v<OutputType, std::vector<Kakshya::DataVariant>>) {
            out.data = std::vector<Kakshya::DataVariant> { std::move(cropped) };
        } else {
            out.data = OperationHelper::reconstruct_from_double<OutputType>({}, {});
        }

        out.dimensions = { Kakshya::DataDimension::spatial_2d(crop_w, crop_h) };
        out.modality = (channels == 1)
            ? Kakshya::DataModality::IMAGE_2D
            : Kakshya::DataModality::IMAGE_COLOR;
        out.metadata = input.metadata;
        out.metadata["crop_w"] = crop_w;
        out.metadata["crop_h"] = crop_h;
        out.metadata["vision_extractor_mode"] = static_cast<int>(m_mode);
        out.metadata["vision_extractor_index"] = m_index;
        return out;
    }

private:
    struct CropPC {
        float src_x, src_y, src_w, src_h;
        uint32_t out_w, out_h;
    };

    VisionExtractionMode m_mode { VisionExtractionMode::BBOX };
    uint32_t m_index { 0 };
};

// ============================================================================
// Aliases
// ============================================================================

/// Default: container in, DataVariant pixels out.
using StandardVisionExtractor = VisionExtractor<
    std::shared_ptr<Kakshya::SignalSourceContainer>,
    std::vector<Kakshya::DataVariant>>;

/// DataVariant pixels in, DataVariant pixels out.
using DataVisionExtractor = VisionExtractor<
    std::vector<Kakshya::DataVariant>,
    std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
