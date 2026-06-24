#pragma once

#include "UniversalAnalyzer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Kinesis/Vision/VisionExecutor.hpp"
#include "MayaFlux/Kinesis/Vision/VisionOp.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

namespace MayaFlux::Yantra {

// ============================================================================
// Result types
// ============================================================================

/**
 * @brief Flattened structured outputs from one VisionResult.
 *
 * Each field is populated only when the terminal VisionSequence step
 * produces the corresponding type. Empty/default otherwise.
 * BoundingBox is surfaced from ComponentResult::boxes directly.
 */
struct MAYAFLUX_API DetectionSummary {
    std::vector<Kinesis::Vision::BoundingBox> boxes;
    std::vector<Kinesis::Vision::Contour> contours;
    std::vector<Kinesis::Vision::Keypoint> keypoints;
    std::vector<Kinesis::Vision::TrackResult> tracks;
    Kinesis::Vision::GradientResult gradient;
    Kinesis::Vision::ComponentResult components;
};

/**
 * @struct VisionAnalysis
 * @brief Analysis result produced by VisionAnalyzer for one frame.
 */
struct MAYAFLUX_API VisionAnalysis {
    DetectionSummary frame;
    std::vector<float> pixel_image;
    uint32_t w { 0 };
    uint32_t h { 0 };
};

/**
 * @brief Extract a named scalar from a VisionAnalysis result.
 *
 * Supported qualifiers:
 *   "box_count"               number of bounding boxes
 *   "contour_count"           number of contours
 *   "keypoint_count"          number of keypoints
 *   "track_count"             number of tracked points
 *   "component_count"         number of connected components
 *   "mean_gradient_magnitude" mean of GradientResult::magnitude
 *
 * Unknown or empty qualifier returns 0.0.
 *
 * @param analysis Result produced by VisionAnalyzer.
 * @param qualifier Name of the scalar to extract.
 */
[[nodiscard]] MAYAFLUX_API double extract_scalar_vision(
    const VisionAnalysis& analysis, const std::string& qualifier);

// ============================================================================
// VisionAnalyzer
// ============================================================================

/**
 * @class VisionAnalyzer
 * @brief UniversalAnalyzer that takes any pixel-bearing input, runs a
 *        VisionSequence via VisionExecutor, and surfaces structured results
 *        into the Yantra compute graph.
 *
 * InputType can be any ComputeData type carrying pixel data:
 *   - std::vector<DataVariant>                 (direct pixel variants)
 *   - std::shared_ptr<SignalSourceContainer>   (TextureContainer, VideoStreamContainer, etc.)
 *   - Kakshya::Region / RegionGroup            (pixel sub-region with container)
 *
 * extract_structured_native handles all unwrapping transparently. Width and
 * height are read from SPATIAL_X / SPATIAL_Y dimension roles in the
 * DataStructureInfo, or from metadata keys "width" and "height" as fallback.
 *
 * OutputType is std::vector<DataVariant> carrying VisionResult::pixel_image
 * as vector<float>, matching the standard analyzer pipeline contract.
 * Structured results (boxes, contours, keypoints, tracks, gradient,
 * components) are stored via store_current_analysis and retrieved with
 * get_vision_analysis() / extract_scalar_vision().
 *
 * @code
 * auto va = std::make_shared<StandardVisionAnalyzer>(sequence);
 * va->apply_operation(pixel_datum);
 * auto analysis = va->get_vision_analysis();
 * double n = extract_scalar_vision(analysis, "box_count");
 * @endcode
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = std::vector<Kakshya::DataVariant>>
class VisionAnalyzer
    : public UniversalAnalyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = UniversalAnalyzer<InputType, OutputType>;

    /**
     * @brief Construct with the VisionSequence to execute each call.
     * @param sequence Ordered VisionSteps describing the pipeline.
     */
    explicit VisionAnalyzer(Kinesis::Vision::VisionSequence sequence)
        : m_sequence(std::move(sequence))
    {
    }

    /**
     * @brief Type-safe vision analysis.
     * @param data Input datum carrying pixel data.
     * @return VisionAnalysis directly.
     */
    VisionAnalysis analyze_vision(const input_type& data)
    {
        this->analyze_data(data);
        return get_vision_analysis();
    }

    VisionAnalysis analyze_vision(const InputType& data)
    {
        return analyze_vision(input_type { data });
    }

    /**
     * @brief Get the last VisionAnalysis result.
     */
    [[nodiscard]] VisionAnalysis get_vision_analysis() const
    {
        return safe_any_cast_or_throw<VisionAnalysis>(this->get_current_analysis());
    }

    /**
     * @brief Replace the pipeline and reset inter-frame executor state.
     *
     * Not thread-safe relative to analyze_implementation. Call only when idle.
     */
    void set_sequence(Kinesis::Vision::VisionSequence sequence)
    {
        m_sequence = std::move(sequence);
        m_executor.reset();
    }

    /**
     * @brief Clear stored optical flow state.
     *
     * Call when the pixel source changes (camera switch, video seek).
     */
    void reset() { m_executor.reset(); }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::SPATIAL;
    }

    [[nodiscard]] std::vector<std::string> get_available_methods() const override
    {
        return { "default" };
    }

protected:
    [[nodiscard]] std::string get_analyzer_name() const override
    {
        return "VisionAnalyzer";
    }

    output_type analyze_implementation(const input_type& input) override
    {
        try {
            auto [native_spans, structure_info] = OperationHelper::extract_structured_native(
                const_cast<input_type&>(input));

            if (native_spans.empty()) {
                error<std::runtime_error>(
                    Journal::Component::Yantra,
                    Journal::Context::ComputeMatrix,
                    std::source_location::current(),
                    "VisionAnalyzer: no pixel data in input");
            }

            uint32_t w = 0;
            uint32_t h = 0;
            for (const auto& dim : structure_info.dimensions) {
                if (dim.role == Kakshya::DataDimension::Role::SPATIAL_X) {
                    w = static_cast<uint32_t>(dim.size);
                } else if (dim.role == Kakshya::DataDimension::Role::SPATIAL_Y) {
                    h = static_cast<uint32_t>(dim.size);
                }
            }
            if (w == 0)
                w = Kakshya::get_metadata_value<uint32_t>(input.metadata, "width").value_or(0);
            if (h == 0)
                h = Kakshya::get_metadata_value<uint32_t>(input.metadata, "height").value_or(0);

            if (w == 0 || h == 0) {
                error<std::runtime_error>(
                    Journal::Component::Yantra,
                    Journal::Context::ComputeMatrix,
                    std::source_location::current(),
                    "VisionAnalyzer: width/height not resolvable from dimensions or metadata");
            }

            auto frame = std::visit([&](const auto& span) -> std::span<const float> {
                using ElemT = typename std::decay_t<decltype(span)>::value_type;
                if constexpr (std::is_same_v<ElemT, float>) {
                    return span;
                } else {
                    using VecT = std::vector<ElemT>;
                    VecT tmp(span.begin(), span.end());
                    Kakshya::DataVariant var(std::move(tmp));
                    return Kakshya::as_normalised_float(var, m_float_storage);
                }
            },
                native_spans[0]);

            if (frame.empty()) {
                error<std::runtime_error>(
                    Journal::Component::Yantra,
                    Journal::Context::ComputeMatrix,
                    std::source_location::current(),
                    "VisionAnalyzer: normalisation produced empty frame");
            }

            const Kinesis::Vision::VisionResult vr = m_executor.run(m_sequence, frame, w, h);

            VisionAnalysis analysis;
            analysis.pixel_image = vr.pixel_image;
            analysis.w = vr.w;
            analysis.h = vr.h;

            std::visit([&](const auto& s) {
                using T = std::decay_t<decltype(s)>;
                if constexpr (std::is_same_v<T, Kinesis::Vision::GradientResult>) {
                    analysis.frame.gradient = s;
                } else if constexpr (std::is_same_v<T, Kinesis::Vision::ComponentResult>) {
                    analysis.frame.components = s;
                    analysis.frame.boxes = s.boxes;
                } else if constexpr (std::is_same_v<T, std::vector<Kinesis::Vision::Contour>>) {
                    analysis.frame.contours = s;
                } else if constexpr (std::is_same_v<T, std::vector<Kinesis::Vision::Keypoint>>) {
                    analysis.frame.keypoints = s;
                } else if constexpr (std::is_same_v<T, std::vector<Kinesis::Vision::TrackResult>>) {
                    analysis.frame.tracks = s;
                }
            },
                vr.structured);

            this->store_current_analysis(analysis);

            return create_pipeline_output(input, vr, structure_info);

        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "VisionAnalyzer: {}", e.what());
            output_type err;
            err.metadata = input.metadata;
            err.metadata["error"] = std::string(e.what());
            return err;
        }
    }

private:
    Kinesis::Vision::VisionSequence m_sequence;
    Kinesis::Vision::VisionExecutor m_executor;
    mutable std::vector<float> m_float_storage;

    output_type create_pipeline_output(
        const input_type& input,
        const Kinesis::Vision::VisionResult& vr,
        const DataStructureInfo& info)
    {
        std::vector<Kakshya::DataVariant> out_variants;
        if (!vr.pixel_image.empty())
            out_variants.emplace_back(vr.pixel_image);

        output_type out = this->convert_result(
            std::vector<std::vector<double>> {}, info);
        out.data = OperationHelper::reconstruct_from_double<OutputType>({}, info);

        if constexpr (std::is_same_v<OutputType, std::vector<Kakshya::DataVariant>>) {
            out.data = std::move(out_variants);
        }

        out.dimensions = {
            Kakshya::DataDimension::spatial_2d(vr.w, vr.h)
        };
        out.modality = Kakshya::DataModality::IMAGE_2D;
        out.metadata = input.metadata;
        out.metadata["source_analyzer"] = std::string("VisionAnalyzer");
        out.metadata["vision_w"] = vr.w;
        out.metadata["vision_h"] = vr.h;
        return out;
    }
};

// ============================================================================
// Aliases
// ============================================================================

/// Standard: DataVariant pixels in, DataVariant pixel_image out.
using StandardVisionAnalyzer = VisionAnalyzer<std::vector<Kakshya::DataVariant>,
    std::vector<Kakshya::DataVariant>>;

/// Container: any SignalSourceContainer in, DataVariant pixel_image out.
using ContainerVisionAnalyzer = VisionAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>,
    std::vector<Kakshya::DataVariant>>;

/// Region: pixel sub-region with container in, DataVariant pixel_image out.
using RegionVisionAnalyzer = VisionAnalyzer<Kakshya::Region,
    std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
