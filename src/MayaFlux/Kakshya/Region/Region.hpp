#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Transitive/Facet/Facets.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#ifdef CALLBACK
#undef CALLBACK
#endif // CALLBACK
#endif // MAYAFLUX_PLATFORM_WINDOWS

namespace MayaFlux::Kakshya {

/**
 * @enum RegionSelectionPattern
 * @brief Describes how regions are selected for processing or playback.
 */
enum class RegionSelectionPattern : uint8_t {
    ALL, ///< Process all regions
    SEQUENTIAL, ///< Process regions in order
    RANDOM, ///< Random selection
    ROUND_ROBIN, ///< Cycle through regions
    WEIGHTED, ///< Weighted random selection
    OVERLAP, ///< Overlapping selection
    EXCLUSIVE, ///< Mutually exclusive selection
    CUSTOM ///< User-defined selection logic
};

/**
 * @enum RegionTransition
 * @brief Describes how transitions between regions are handled.
 */
enum class RegionTransition : uint8_t {
    IMMEDIATE, ///< No transition, jump directly
    CROSSFADE, ///< Crossfade between regions
    OVERLAP, ///< Overlap regions during transition
    GATED, ///< Hard gate between regions
    CALLBACK ///< Use callback for custom transition
};

/**
 * @enum RegionState
 * @brief Processing state for regions.
 */
enum class RegionState : uint8_t {
    IDLE, ///< Not being processed
    LOADING, ///< Data being loaded
    READY, ///< Ready for processing
    ACTIVE, ///< Currently being processed
    TRANSITIONING, ///< In transition to another region
    UNLOADING ///< Being removed from memory
};

/**
 * @struct Region
 * @brief Represents a point or span in N-dimensional space.
 *
 * Regions represent precise locations or segments within signal data,
 * defined by start and end frame positions. Each region can carry typed
 * values stored under Facet keys, allowing for rich metadata association
 * with each point.
 *
 * Common DSP-specific uses include:
 * - Marking transients and onset detection points
 * - Identifying spectral features or frequency domain events
 * - Defining zero-crossing boundaries for phase analysis
 * - Marking signal transformation points (e.g., filter application boundaries)
 * - Storing analysis results like RMS peaks, harmonic content points, or noise floors
 *
 * The typed value system allows for storing any computed values or metadata
 * associated with specific signal locations, enabling advanced signal processing
 * workflows and algorithmic decision-making.
 */
struct MAYAFLUX_API Region {
    /** @brief Starting frame index (inclusive) */
    std::vector<uint64_t> start_coordinates;

    /** @brief Ending frame index (inclusive) */
    std::vector<uint64_t> end_coordinates;

    /** @brief Data modality of this region, when it is set. */
    std::optional<DataModality> modality;

    /** @brief Display label, when one was given. */
    std::optional<std::string> label;

    /** @brief Returns a typed attribute, or nullptr when absent. */
    template <typename T>
    [[nodiscard]] const T* get(const Facet<T>& key) const
    {
        return m_facets.get(key);
    }

    /** @brief Returns the struct stored under its type, or nullptr. */
    template <typename T>
    [[nodiscard]] const T* get() const
    {
        return m_facets.get<T>();
    }

    /** @brief Stores a typed attribute under its Facet key. */
    template <typename T>
    void set(const Facet<T>& key, std::type_identity_t<T> value)
    {
        m_facets.set(key, std::move(value));
    }

    /** @brief Stores a struct under its type identity. */
    template <typename T>
    void set(T value)
    {
        m_facets.set(std::move(value));
    }

    /** @brief Replaces all typed attributes with @p source. Other Region members are unchanged. */
    void replace(Facets source)
    {
        m_facets = std::move(source);
    }

    Region() = default;

    /**
     * @brief Construct a point-like region (single coordinate).
     * @param coordinates The N-dimensional coordinates.
     */
    Region(const std::vector<uint64_t>& coordinates)
        : start_coordinates(coordinates)
        , end_coordinates(coordinates)
    {
    }

    /**
     * @brief Construct a span-like region (start and end coordinates).
     * @param start Start coordinates (inclusive).
     * @param end End coordinates (inclusive).
     */
    Region(std::vector<uint64_t> start,
        std::vector<uint64_t> end)
        : start_coordinates(std::move(start))
        , end_coordinates(std::move(end))
    {
    }

    /**
     * @brief Create a Region representing a single time point (e.g., a frame or sample).
     * @param frame The frame index (time coordinate).
     * @param label Optional label for this point.
     * @return Region at the specified frame.
     */
    static Region time_point(uint64_t frame,
        const std::string& label = "")
    {
        return with_label(Region(std::vector<uint64_t> { frame }), label);
    }

    /**
     * @brief Create a Region representing a time span (e.g., a segment of frames).
     * @param start_frame Start frame index (inclusive).
     * @param end_frame End frame index (inclusive).
     * @param label Optional label for this span.
     * @return Region covering the specified time span.
     */
    static Region time_span(uint64_t start_frame,
        uint64_t end_frame,
        const std::string& label = "")
    {
        return with_label(Region({ start_frame }, { end_frame }), label);
    }

    /**
     * @brief Create a Region for a single audio sample/channel location.
     * @param frame The frame index (time).
     * @param channel The channel index.
     * @param label Optional label for this region.
     * @return Region at the specified frame/channel.
     */
    static Region audio_point(uint64_t frame,
        uint32_t channel,
        const std::string& label = "")
    {
        Region region = with_label(Region({ frame, channel }, { frame, channel }), label);
        region.modality = DataModality::AUDIO_1D;
        return region;
    }

    /**
     * @brief Create a Region representing a span in audio (frames and channels).
     * @param start_frame Start frame index (inclusive).
     * @param end_frame End frame index (inclusive).
     * @param start_channel Start channel index (inclusive).
     * @param end_channel End channel index (inclusive).
     * @param label Optional label for this region.
     * @return Region covering the specified audio region.
     */
    static Region audio_span(uint64_t start_frame,
        uint64_t end_frame,
        uint32_t start_channel,
        uint32_t end_channel,
        const std::string& label = "")
    {
        Region region = with_label(
            Region({ start_frame, start_channel }, { end_frame, end_channel }), label);
        region.modality = DataModality::AUDIO_MULTICHANNEL;
        return region;
    }

    /**
     * @brief Create a Region representing a rectangular region in an image.
     * @param x1 Top-left X coordinate.
     * @param y1 Top-left Y coordinate.
     * @param x2 Bottom-right X coordinate.
     * @param y2 Bottom-right Y coordinate.
     * @param label Optional label for this rectangle.
     * @return Region covering the specified image rectangle.
     */
    static Region image_rect(uint64_t x1, uint64_t y1,
        uint64_t x2, uint64_t y2,
        const std::string& label = "")
    {
        return with_label(Region({ x1, y1 }, { x2, y2 }), label);
    }

    /**
     * @brief Create a Region representing a region in a video (frames and spatial rectangle).
     * @param start_frame Start frame index (inclusive).
     * @param end_frame End frame index (inclusive).
     * @param x1 Top-left X coordinate.
     * @param y1 Top-left Y coordinate.
     * @param x2 Bottom-right X coordinate.
     * @param y2 Bottom-right Y coordinate.
     * @param label Optional label for this region.
     * @return Region covering the specified video region.
     */
    static Region video_region(uint64_t start_frame,
        uint64_t end_frame,
        uint64_t x1, uint64_t y1,
        uint64_t x2, uint64_t y2,
        const std::string& label = "")
    {
        return with_label(Region({ start_frame, x1, y1 }, { end_frame, x2, y2 }), label);
    }

    /**
     * @brief Check if this region is a single point (start == end).
     */
    bool is_point() const
    {
        return start_coordinates == end_coordinates;
    }

    /**
     * @brief Check if the given coordinates are contained within this region.
     * @param coordinates N-dimensional coordinates to check.
     * @return True if contained, false otherwise.
     */
    bool contains(const std::vector<uint64_t>& coordinates) const
    {
        if (coordinates.size() != start_coordinates.size())
            return false;

        for (size_t i = 0; i < start_coordinates.size(); i++) {
            if (coordinates[i] < start_coordinates[i] || coordinates[i] > end_coordinates[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Check if this region overlaps with another region.
     * @param other The other region.
     * @return True if overlapping, false otherwise.
     */
    bool overlaps(const Region& other) const
    {
        if (start_coordinates.size() != other.start_coordinates.size())
            return false;

        for (size_t i = 0; i < start_coordinates.size(); i++) {
            if (end_coordinates[i] < other.start_coordinates[i] || start_coordinates[i] > other.end_coordinates[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get the span (length) of the region along a dimension.
     * @param dimension_index The dimension to query.
     * @return The span (number of elements) along the dimension.
     */
    uint64_t get_span(uint32_t dimension_index = 0) const
    {
        if (dimension_index >= start_coordinates.size())
            return 0;
        return end_coordinates[dimension_index] - start_coordinates[dimension_index] + 1;
    }

    /**
     * @brief Get the total volume (number of elements) in the region.
     * @return The product of spans across all dimensions.
     */
    uint64_t get_volume() const
    {
        uint64_t volume = 1;
        for (size_t i = 0; i < start_coordinates.size(); i++) {
            volume *= get_span(i);
        }
        return volume;
    }

    /**
     * @brief Get the duration (span) along a specific dimension.
     * @param dimension_index The dimension to query.
     * @return The duration (number of elements) along the dimension.
     */
    uint64_t get_duration(uint32_t dimension_index = 0) const
    {
        if (dimension_index >= start_coordinates.size()) {
            return 0;
        }
        return end_coordinates[dimension_index] - start_coordinates[dimension_index] + 1;
    }

    /**
     * @brief Sets the label when one is given.
     */
    static Region with_label(Region region, const std::string& label)
    {
        if (!label.empty())
            region.label = label;
        return region;
    }

    /**
     * @brief Translate the region by an offset vector.
     * @param offset The offset for each dimension (can be negative).
     * @return A new translated Region.
     */
    Region translate(const std::vector<int64_t>& offset) const
    {
        Region result = *this;
        for (size_t i = 0; i < std::min(offset.size(), start_coordinates.size()); i++) {
            if (offset[i] < 0 && std::abs(offset[i]) > static_cast<int64_t>(result.start_coordinates[i])) {
                result.start_coordinates[i] = 0;
            } else {
                result.start_coordinates[i] += offset[i];
            }

            if (offset[i] < 0 && std::abs(offset[i]) > static_cast<int64_t>(result.end_coordinates[i])) {
                result.end_coordinates[i] = 0;
            } else {
                result.end_coordinates[i] += offset[i];
            }
        }
        return result;
    }

    /**
     * @brief Scale the region about its center by the given factors.
     * @param factors Scaling factors for each dimension.
     * @return A new scaled Region.
     */
    Region scale(const std::vector<double>& factors) const
    {
        Region result = *this;
        for (size_t i = 0; i < std::min<size_t>(factors.size(), start_coordinates.size()); i++) {
            uint64_t center = (start_coordinates[i] + end_coordinates[i]) / 2;
            uint64_t half_span = get_span(i) / 2;
            auto new_half_span = static_cast<uint64_t>((double)half_span * factors[i]);

            result.start_coordinates[i] = center - new_half_span;
            result.end_coordinates[i] = center + new_half_span;
        }
        return result;
    }

    /**
     * @brief Equality comparison for Regions.
     */
    bool operator==(const Region& other) const
    {
        if (start_coordinates.size() != other.start_coordinates.size() || end_coordinates.size() != other.end_coordinates.size()) {
            return false;
        }

        for (size_t i = 0; i < start_coordinates.size(); ++i) {
            if (start_coordinates[i] != other.start_coordinates[i]) {
                return false;
            }
        }

        for (size_t i = 0; i < end_coordinates.size(); ++i) {
            if (end_coordinates[i] != other.end_coordinates[i]) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Inequality comparison for Regions.
     */
    bool operator!=(const Region& other) const
    {
        return !(*this == other);
    }

private:
    friend struct OrganizedRegion;
    Facets m_facets;
};

} // namespace MayaFlux::Kakshya
