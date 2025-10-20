#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

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
 * defined by start and end frame positions. Each region can have additional
 * attributes stored in a flexible key-value map, allowing for rich metadata
 * association with each point.
 *
 * Common DSP-specific uses include:
 * - Marking transients and onset detection points
 * - Identifying spectral features or frequency domain events
 * - Defining zero-crossing boundaries for phase analysis
 * - Marking signal transformation points (e.g., filter application boundaries)
 * - Storing analysis results like RMS peaks, harmonic content points, or noise floors
 *
 * The flexible attribute system allows for storing any computed values or metadata
 * associated with specific signal locations, enabling advanced signal processing
 * workflows and algorithmic decision-making.
 */
struct MAYAFLUX_API Region {
    /** @brief Starting frame index (inclusive) */
    std::vector<uint64_t> start_coordinates;

    /** @brief Ending frame index (inclusive) */
    std::vector<uint64_t> end_coordinates;

    /** @brief Flexible key-value store for region-specific attributes */
    std::unordered_map<std::string, std::any> attributes;

    Region() = default;

    /**
     * @brief Construct a point-like region (single coordinate).
     * @param coordinates The N-dimensional coordinates.
     * @param attributes Optional metadata.
     */
    Region(const std::vector<uint64_t>& coordinates,
        std::unordered_map<std::string, std::any> attributes = {})
        : start_coordinates(coordinates)
        , end_coordinates(coordinates)
        , attributes(std::move(attributes))
    {
    }

    /**
     * @brief Construct a span-like region (start and end coordinates).
     * @param start Start coordinates (inclusive).
     * @param end End coordinates (inclusive).
     * @param attributes Optional metadata.
     */
    Region(std::vector<uint64_t> start,
        std::vector<uint64_t> end,
        std::unordered_map<std::string, std::any> attributes = {})
        : start_coordinates(std::move(start))
        , end_coordinates(std::move(end))
        , attributes(std::move(attributes))
    {
    }

    /**
     * @brief Create a Region representing a single time point (e.g., a frame or sample).
     * @param frame The frame index (time coordinate).
     * @param label Optional label for this point.
     * @param extra_data Optional extra metadata (e.g., analysis result).
     * @return Region at the specified frame.
     */
    static Region time_point(uint64_t frame,
        const std::string& label = "",
        const std::any& extra_data = {})
    {
        std::unordered_map<std::string, std::any> attrs;
        if (!label.empty())
            attrs["label"] = label;
        if (extra_data.has_value())
            attrs["data"] = extra_data;
        attrs["type"] = "time_point";
        return Region({ frame }, attrs);
    }

    /**
     * @brief Create a Region representing a time span (e.g., a segment of frames).
     * @param start_frame Start frame index (inclusive).
     * @param end_frame End frame index (inclusive).
     * @param label Optional label for this span.
     * @param extra_data Optional extra metadata.
     * @return Region covering the specified time span.
     */
    static Region time_span(uint64_t start_frame,
        uint64_t end_frame,
        const std::string& label = "",
        const std::any& extra_data = {})
    {
        std::unordered_map<std::string, std::any> attrs;
        if (!label.empty())
            attrs["label"] = label;
        if (extra_data.has_value())
            attrs["data"] = extra_data;
        attrs["type"] = "time_span";
        return Region({ start_frame }, { end_frame }, attrs);
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
        std::unordered_map<std::string, std::any> attrs;
        if (!label.empty())
            attrs["label"] = label;
        attrs["type"] = "audio_point";
        return Region({ frame, channel }, attrs);
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
        std::unordered_map<std::string, std::any> attrs;
        if (!label.empty())
            attrs["label"] = label;
        attrs["type"] = "audio_region";
        return Region({ start_frame, start_channel },
            { end_frame, end_channel }, attrs);
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
        std::unordered_map<std::string, std::any> attrs;
        if (!label.empty())
            attrs["label"] = label;
        attrs["type"] = "image_rect";
        return Region({ x1, y1 }, { x2, y2 }, attrs);
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
        std::unordered_map<std::string, std::any> attrs;
        if (!label.empty())
            attrs["label"] = label;
        attrs["type"] = "video_region";
        return Region({ start_frame, x1, y1 },
            { end_frame, x2, y2 }, attrs);
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
     * @brief Get an attribute value by key, with type conversion support.
     * @tparam T The expected type.
     * @param key The attribute key.
     * @return Optional value if present and convertible.
     */
    template <typename T>
    std::optional<T> get_attribute(const std::string& key) const
    {
        auto it = attributes.find(key);
        if (it == attributes.end()) {
            return std::nullopt;
        }

        return safe_any_cast<T>(it->second);
    }

    /**
     * @brief Set an attribute value by key.
     * @param key The attribute key.
     * @param value The value to set.
     */
    void set_attribute(const std::string& key, std::any value)
    {
        attributes[key] = std::move(value);
    }

    /**
     * @brief Get the label attribute (if present).
     * @return The label string, or empty if not set.
     */
    std::string get_label() const
    {
        auto label = get_attribute<std::string>("label");
        return label.value_or("");
    }

    /**
     * @brief Set the label attribute.
     * @param label The label string.
     */
    void set_label(const std::string& label)
    {
        set_attribute("label", label);
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
};

} // namespace MayaFlux::Kakshya
