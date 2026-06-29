#pragma once

#include "UniversalSorter.hpp"

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Yantra/Analyzers/VisionAnalyzer.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum VisionSortKey
 * @brief Named scalar attributes on vision detection results used as sort keys.
 *
 * Applied to whichever collection is populated in VisionAnalysis::frame:
 * boxes are addressed by BoundingBox fields, contours by Contour fields.
 * CENTROID_X and CENTROID_Y on boxes use the box centre; on contours they
 * use the mean of the contour point set.
 */
enum class VisionSortKey : uint8_t {
    AREA, ///< Normalised area. BoundingBox: w*h. Contour: Contour::area.
    PERIMETER, ///< Normalised perimeter. BoundingBox: 2*(w+h). Contour: Contour::perimeter.
    CENTROID_X, ///< Normalised x coordinate of the detection centre.
    CENTROID_Y, ///< Normalised y coordinate of the detection centre.
    WIDTH, ///< Normalised width. BoundingBox: w. Contour: tight bbox width.
    HEIGHT, ///< Normalised height. BoundingBox: h. Contour: tight bbox height.
};

/**
 * @class VisionSorter
 * @brief Reorders detection results in a VisionAnalysis by a scalar key or callable.
 *
 * Reads VisionAnalysis from input.metadata["vision_analysis"], sorts the
 * populated detection collection (boxes or contours) according to the configured
 * key and direction, writes the reordered VisionAnalysis back to
 * output.metadata["vision_analysis"], and passes input.data through unmodified.
 *
 * The callable overload receives the element index and the full VisionAnalysis,
 * giving access to pixel data and all structured fields for cross-modal keys.
 * When both a key and a callable are set, the callable takes precedence.
 *
 * SortingDirection::ASCENDING places the smallest key value at index 0.
 * SortingDirection::DESCENDING places the largest at index 0, which is the
 * natural input for VisionExtractor when "take the dominant detection" is the goal.
 *
 * @code
 * auto sorter = std::make_shared<VisionSorter>(
 *     VisionSortKey::AREA, SortingDirection::DESCENDING);
 *
 * Datum<shared_ptr<SignalSourceContainer>> d { container };
 * d.metadata["vision_analysis"] = analyzer->analyze_vision(frame_datum);
 * auto sorted = sorter->apply_operation(d);
 * // VisionExtractor at index 0 now receives the largest detection.
 * @endcode
 */
class MAYAFLUX_API VisionSorter
    : public UniversalSorter<
          std::shared_ptr<Kakshya::SignalSourceContainer>,
          std::shared_ptr<Kakshya::SignalSourceContainer>> {
public:
    using input_type = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>;
    using output_type = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>;

    /**
     * @brief Callable type for custom sort keys.
     *
     * Receives the zero-based element index within the detection collection
     * and the full VisionAnalysis. Returns a float used as the sort key.
     * Lower values sort earlier in ASCENDING order.
     */
    using KeyFn = std::function<float(size_t index, const VisionAnalysis&)>;

    /**
     * @brief Construct with a named key and direction.
     * @param key       Attribute to sort by.
     * @param direction ASCENDING puts smallest first; DESCENDING puts largest first.
     */
    explicit VisionSorter(
        VisionSortKey key = VisionSortKey::AREA,
        SortingDirection direction = SortingDirection::DESCENDING)
        : m_key(key)
    {
        this->set_direction(direction);
        this->set_strategy(SortingStrategy::COPY_SORT);
        this->set_granularity(SortingGranularity::RAW_DATA);
    }

    /**
     * @brief Construct with a callable key and direction.
     * @param key_fn    Callable returning a float sort key per element.
     * @param direction ASCENDING puts smallest first; DESCENDING puts largest first.
     */
    explicit VisionSorter(
        KeyFn key_fn,
        SortingDirection direction = SortingDirection::DESCENDING)
        : m_key_fn(std::move(key_fn))
    {
        this->set_direction(direction);
        this->set_strategy(SortingStrategy::COPY_SORT);
        this->set_granularity(SortingGranularity::RAW_DATA);
    }

    /** @brief Replace the named key. Clears any active callable. */
    void set_key(VisionSortKey key)
    {
        m_key = key;
        m_key_fn = nullptr;
    }

    /** @brief Replace the callable key. Takes precedence over the named key. */
    void set_key_fn(KeyFn key_fn) { m_key_fn = std::move(key_fn); }

    [[nodiscard]] VisionSortKey get_key() const { return m_key; }
    [[nodiscard]] bool has_key_fn() const { return m_key_fn != nullptr; }

    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::SPATIAL;
    }

    [[nodiscard]] std::vector<std::string> get_available_methods() const
    {
        return Reflect::get_enum_names_lowercase<VisionSortKey>();
    }

protected:
    [[nodiscard]] std::string get_sorter_name() const override
    {
        return "VisionSorter";
    }

    output_type sort_implementation(const input_type& input) override;

private:
    VisionSortKey m_key { VisionSortKey::AREA };
    KeyFn m_key_fn;

    /**
     * @brief Compute the named key value for a BoundingBox.
     */
    [[nodiscard]] float key_for_box(
        const Kinesis::Vision::BoundingBox& box) const;

    /**
     * @brief Compute the named key value for a Contour.
     */
    [[nodiscard]] float key_for_contour(
        const Kinesis::Vision::Contour& contour) const;
};

} // namespace MayaFlux::Yantra
