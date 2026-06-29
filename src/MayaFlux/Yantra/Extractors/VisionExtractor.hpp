#pragma once

#include "UniversalExtractor.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum VisionExtractionMode
 * @brief Selects the spatial mask used to crop pixels from a container.
 */
enum class VisionExtractionMode : uint8_t {
    BBOX, ///< Crop the axis-aligned rectangle of a BoundingBox.
    CONTOUR_TIGHT, ///< Crop the tight bounding rect of a Contour; no polygon mask applied.
    CONTOUR_MASKED, ///< Crop the tight bounding rect of a Contour and zero pixels outside the polygon.
};

/**
 * @class VisionExtractor
 * @brief Extracts a pixel sub-region from a container guided by a VisionAnalysis.
 *
 * Consumes a Datum<shared_ptr<SignalSourceContainer>> whose metadata carries a
 * VisionAnalysis under the key "vision_analysis". Uses the bounding boxes or
 * contours in that analysis to address a pixel rectangle via the container's
 * existing get_region_data() path, then wraps the result in a new TextureContainer.
 *
 * The analysis is attached to the Datum by the caller after running a VisionAnalyzer:
 * @code
 * auto analysis = analyzer->analyze_vision(frame_datum);
 * Datum<shared_ptr<SignalSourceContainer>> d { container };
 * d.metadata["vision_analysis"] = analysis;
 * auto patch = extractor->apply_operation(d);
 * @endcode
 *
 * The output is a Datum<shared_ptr<SignalSourceContainer>> carrying a TextureContainer
 * whose pixels are the cropped region. Modality is IMAGE_COLOR or IMAGE_2D depending
 * on channel count. The result is a first-class container: it can be piped into another
 * VisionAnalyzer, fed to a TextureBuffer, or passed to Forma.
 *
 * @note The container's get_region_data() returns the same element type as the
 *       underlying storage (uint8_t, float, etc.). The TextureContainer is
 *       constructed with a matching format inferred from value_element_type().
 */
class MAYAFLUX_API VisionExtractor
    : public UniversalExtractor<
          std::shared_ptr<Kakshya::SignalSourceContainer>,
          std::shared_ptr<Kakshya::SignalSourceContainer>> {
public:
    using input_type = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>;
    using output_type = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>;

    /**
     * @brief Construct with extraction mode and target index.
     * @param mode  Which detection result type and masking strategy to use.
     * @param index Zero-based index into the boxes or contours vector in VisionAnalysis.
     */
    explicit VisionExtractor(VisionExtractionMode mode = VisionExtractionMode::BBOX,
        uint32_t index = 0)
        : m_mode(mode)
        , m_index(index)
    {
    }

    /**
     * @brief Replace the mode without constructing a new extractor.
     */
    void set_mode(VisionExtractionMode mode) { m_mode = mode; }

    /**
     * @brief Replace the target index without constructing a new extractor.
     */
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

protected:
    output_type extract_implementation(const input_type& input) override;

private:
    VisionExtractionMode m_mode { VisionExtractionMode::BBOX };
    uint32_t m_index { 0 };

    /**
     * @brief Infer ImageFormat from the container's value_element_type().
     *
     * Returns RGBA8_UNORM for uint8_t, RGBA32_SFLOAT for float, and
     * RGBA8_UNORM as a safe default otherwise.
     */
    [[nodiscard]] static Portal::Graphics::ImageFormat infer_format(
        const Kakshya::SignalSourceContainer& container);
};

} // namespace MayaFlux::Yantra
