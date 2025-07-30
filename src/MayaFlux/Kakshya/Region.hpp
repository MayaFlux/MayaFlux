#pragma once

#include "NDimensionalContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @enum RegionSelectionPattern
 * @brief Describes how regions are selected for processing or playback.
 */
enum class RegionSelectionPattern {
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
enum class RegionTransition {
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
enum class RegionState {
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
struct Region {
    /** @brief Starting frame index (inclusive) */
    std::vector<u_int64_t> start_coordinates;

    /** @brief Ending frame index (inclusive) */
    std::vector<u_int64_t> end_coordinates;

    /** @brief Flexible key-value store for region-specific attributes */
    std::unordered_map<std::string, std::any> attributes;

    Region() = default;

    /**
     * @brief Construct a point-like region (single coordinate).
     * @param coordinates The N-dimensional coordinates.
     * @param attributes Optional metadata.
     */
    Region(std::vector<u_int64_t> coordinates,
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
    Region(std::vector<u_int64_t> start,
        std::vector<u_int64_t> end,
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
    static Region time_point(u_int64_t frame,
        const std::string& label = "",
        std::any extra_data = {})
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
    static Region time_span(u_int64_t start_frame,
        u_int64_t end_frame,
        const std::string& label = "",
        std::any extra_data = {})
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
    static Region audio_point(u_int64_t frame,
        u_int32_t channel,
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
    static Region audio_span(u_int64_t start_frame,
        u_int64_t end_frame,
        u_int32_t start_channel,
        u_int32_t end_channel,
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
    static Region image_rect(u_int64_t x1, u_int64_t y1,
        u_int64_t x2, u_int64_t y2,
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
    static Region video_region(u_int64_t start_frame,
        u_int64_t end_frame,
        u_int64_t x1, u_int64_t y1,
        u_int64_t x2, u_int64_t y2,
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
    bool contains(const std::vector<u_int64_t>& coordinates) const
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
    u_int64_t get_span(u_int32_t dimension_index = 0) const
    {
        if (dimension_index >= start_coordinates.size())
            return 0;
        return end_coordinates[dimension_index] - start_coordinates[dimension_index] + 1;
    }

    /**
     * @brief Get the total volume (number of elements) in the region.
     * @return The product of spans across all dimensions.
     */
    u_int64_t get_volume() const
    {
        u_int64_t volume = 1;
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
    u_int64_t get_duration(u_int32_t dimension_index = 0) const
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
        for (size_t i = 0; i < std::min(factors.size(), start_coordinates.size()); i++) {
            u_int64_t center = (start_coordinates[i] + end_coordinates[i]) / 2;
            u_int64_t half_span = get_span(i) / 2;
            u_int64_t new_half_span = static_cast<u_int64_t>(half_span * factors[i]);

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

/**
 * @struct RegionCache
 * @brief Stores cached data for a region, with metadata for cache management.
 */
struct RegionCache {
    DataVariant data; ///< Cached data
    Region source_region; ///< Region this cache corresponds to
    std::chrono::steady_clock::time_point load_time; ///< When cache was loaded
    size_t access_count = 0; ///< Number of times accessed
    bool is_dirty = false; ///< Whether cache is dirty

    void mark_accessed()
    {
        access_count++;
    }

    void mark_dirty()
    {
        is_dirty = true;
    }

    std::chrono::duration<double> age() const
    {
        return std::chrono::steady_clock::now() - load_time;
    }
};

/**
 * @struct RegionSegment
 * @brief Represents a discrete segment of audio data with caching capabilities.
 *
 * Defines a time-bounded segment of audio that can be cached for efficient
 * playback and manipulation in non-linear processing contexts.
 */
struct RegionSegment {
    Region source_region; ///< Associated region
    std::vector<u_int64_t> offset_in_region; ///< Offset within the source region
    std::vector<u_int64_t> segment_size; ///< Size in each dimension

    RegionCache cache; ///< Multi-channel cached audio data
    bool is_cached = false; ///< Flag indicating if data is cached

    std::vector<u_int64_t> current_position { 0 }; ///< Current position within segment
    bool is_active = false;
    RegionState state = RegionState::IDLE;

    std::unordered_map<std::string, std::any> processing_metadata; ///< Arbitrary processing metadata

    RegionSegment() = default;

    /**
     * @brief Construct a segment from a region (full region).
     * @param region The source region.
     */
    explicit RegionSegment(const Region& region)
        : source_region(region)
        , offset_in_region(region.start_coordinates.size(), 0)
        , segment_size(region.start_coordinates.size())
        , current_position(region.start_coordinates.size(), 0)
    {
        for (size_t i = 0; i < segment_size.size(); ++i) {
            segment_size[i] = region.get_span(i);
        }
    }

    /**
     * @brief Construct a segment from a region with offset and size.
     * @param region The source region.
     * @param offset Offset within the region.
     * @param size Size of the segment.
     */
    RegionSegment(const Region& region,
        const std::vector<u_int64_t>& offset,
        const std::vector<u_int64_t>& size)
        : source_region(region)
        , offset_in_region(offset)
        , segment_size(size)
        , current_position(size.size(), 0)
    {
    }

    /**
     * @brief Get the total number of elements in the segment.
     */
    u_int64_t get_total_elements() const
    {
        u_int64_t total = 1;
        for (auto s : segment_size) {
            total *= s;
        }
        return total;
    }

    /**
     * @brief Check if a position is within this segment.
     * @param pos The N-dimensional position.
     * @return True if contained, false otherwise.
     */
    bool contains_position(const std::vector<u_int64_t>& pos) const
    {
        if (pos.size() != offset_in_region.size())
            return false;

        for (size_t i = 0; i < pos.size(); ++i) {
            if (pos[i] < offset_in_region[i] || pos[i] >= offset_in_region[i] + segment_size[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get the age of the cache in seconds.
     * @return Age in seconds, or -1 if not cached.
     */
    double get_cache_age_seconds() const
    {
        if (!is_cached)
            return -1.0;
        return cache.age().count();
    }

    /* bool has_data_for_dimension(size_t dimension) const
    {
        return dimension < cached_data.size() && !get_cached_element_size(dimension);
    } */

    // Get data for a specific dimension
    /* const DataVariant& get_dimension_data(size_t dimension) const
    {
        static const DataVariant temp;
        return dimension < cached_data.size() ? cached_data[dimension] : temp;
    } */

    void mark_active()
    {
        is_active = true;
        state = RegionState::ACTIVE;
    }

    void mark_inactive()
    {
        is_active = false;
        state = RegionState::IDLE;
    }

    /**
     * @brief Mark this segment as cached and store the data.
     * @param data The cached data.
     */
    void mark_cached(const DataVariant& data)
    {
        cache.data = data;
        cache.source_region = source_region;
        is_cached = true;
        cache.load_time = std::chrono::steady_clock::now();
        cache.is_dirty = false;
        state = RegionState::READY;
    }

    /**
     * @brief Clear the cache for this segment.
     */
    void clear_cache()
    {
        cache.data = DataVariant {};
        is_cached = false;
        if (state == RegionState::READY) {
            state = RegionState::IDLE;
        }
    }

    /**
     * @brief Reset the current position within the segment.
     */
    void reset_position()
    {
        std::fill(current_position.begin(), current_position.end(), 0);
    }

    /**
     * @brief Advance the current position within the segment.
     * @param steps Number of steps to advance.
     * @param dimension Dimension to advance.
     * @return True if not at end, false if at end.
     */
    bool advance_position(u_int64_t steps = 1, u_int32_t dimension = 0)
    {
        if (dimension >= current_position.size())
            return false;

        current_position[dimension] += steps;

        // Handle overflow into next dimensions
        for (size_t dim = dimension; dim < current_position.size() - 1; ++dim) {
            if (current_position[dim] >= segment_size[dim]) {
                current_position[dim] = 0;
                current_position[dim + 1]++;
            } else {
                break;
            }
        }

        // Check if we've reached the end
        return current_position.back() < segment_size.back();
    }

    /**
     * @brief Check if the current position is at the end of the segment.
     */
    bool is_at_end() const
    {
        if (current_position.empty())
            return true;
        return current_position.back() >= segment_size.back();
    }

    /**
     * @brief Set processing metadata for this segment.
     * @param key Metadata key.
     * @param value Metadata value.
     */
    void set_processing_metadata(const std::string& key, std::any value)
    {
        processing_metadata[key] = std::move(value);
    }

    /**
     * @brief Get processing metadata for this segment.
     * @tparam T Expected type.
     * @param key Metadata key.
     * @return Optional value if present and convertible.
     */
    template <typename T>
    std::optional<T> get_processing_metadata(const std::string& key) const
    {
        auto it = processing_metadata.find(key);
        if (it != processing_metadata.end()) {
            return safe_any_cast<T>(it->second);
        }
        return std::nullopt;
    }
};

/**
 * @struct RegionGroup
 * @brief Organizes related signal regions into a categorized collection.
 *
 * RegionGroups provide a way to categorize and organize related regions within
 * signal data based on algorithmic or analytical criteria. Each group has a name
 * and can contain multiple Regions, as well as group-level attributes that
 * apply to the entire collection.
 *
 * Common DSP-specific applications include:
 * - Grouping frequency-domain features (e.g., "formants", "resonances", "harmonics")
 * - Categorizing time-domain events (e.g., "transients", "steady_states", "decays")
 * - Organizing analysis results (e.g., "zero_crossings", "spectral_centroids")
 * - Defining processing boundaries (e.g., "convolution_segments", "filter_regions")
 * - Storing algorithmic detection results (e.g., "noise_gates", "compression_thresholds")
 *
 * This data-driven approach enables sophisticated signal processing workflows
 * where algorithms can operate on categorized signal segments without requiring
 * predefined musical or content-specific structures.
 */
struct RegionGroup {
    /** @brief Descriptive name of the group */
    std::string name;

    /** @brief Collection of regions belonging to this group */
    std::vector<Region> regions;

    /** @brief Flexible key-value store for group-specific attributes */
    std::unordered_map<std::string, std::any> attributes;

    RegionState state = RegionState::IDLE;
    RegionTransition transition_type = RegionTransition::IMMEDIATE;
    RegionSelectionPattern region_selection_pattern = RegionSelectionPattern::SEQUENTIAL;
    double transition_duration_ms = 0.0;

    size_t current_region_index = 0;
    std::vector<size_t> active_indices;

    // Optional processing callbacks
    std::function<void(const Region&)> on_region_start;
    std::function<void(const Region&)> on_region_end;
    std::function<void(const Region&, const Region&)> on_transition;

    RegionGroup() = default;

    /**
     * @brief Construct a region group.
     * @param group_name Name of the group.
     * @param regions Regions to include.
     * @param attrs Optional group-level attributes.
     */
    RegionGroup(std::string group_name,
        std::vector<Region> regions = {},
        std::unordered_map<std::string, std::any> attrs = {})
        : name(std::move(group_name))
        , regions(std::move(regions))
        , attributes(std::move(attrs))
    {
    }

    /* static RegionGroup create_sequential_group(std::string group_name,
        std::vector<Region> regions,
        std::unordered_map<std::string, std::any> attributes = {})
    {
        RegionGroup group(group_name, regions, attributes);
        group.region_selection_pattern = RegionSelectionPattern::SEQUENTIAL;
        group.set_attribute("region_selection_pattern", RegionSelectionPattern::SEQUENTIAL);
        return group;
    }

    static RegionGroup create_random_pool(std::string group_name,
        std::vector<Region> regions,
        std::unordered_map<std::string, std::any> attributes = {})
    {
        RegionGroup group(group_name, regions, attributes);
        group.region_selection_pattern = RegionSelectionPattern::RANDOM;
        group.set_attribute("region_selection_pattern", RegionSelectionPattern::RANDOM);
        return group;
    } */

    void add_region(const Region& region)
    {
        regions.push_back(region);
    }

    /**
     * @brief Insert a region at a specific index.
     * @param index Position to insert at.
     * @param region The Region to insert.
     */
    void insert_region(size_t index, const Region& region)
    {
        if (index >= regions.size()) {
            regions.push_back(region);
        } else {
            regions.insert(regions.begin() + index, region);
        }
    }

    /**
     * @brief Remove a region by index.
     * @param index Index of the region to remove.
     */
    void remove_region(size_t index)
    {
        if (index < regions.size()) {
            regions.erase(regions.begin() + index);
            if (current_region_index >= regions.size() && !regions.empty()) {
                current_region_index = regions.size() - 1;
            }
        }
    }

    /**
     * @brief Remove all regions from the group.
     */
    void clear_regions()
    {
        regions.clear();
        current_region_index = 0;
        active_indices.clear();
    }

    /**
     * @brief Sort region by a specific dimension.
     * @param dimension_index The dimension to sort by.
     */
    void sort_by_dimension(size_t dimension_index)
    {
        std::sort(regions.begin(), regions.end(),
            [dimension_index](const Region& a, const Region& b) {
                if (dimension_index < a.start_coordinates.size() && dimension_index < b.start_coordinates.size()) {
                    return a.start_coordinates[dimension_index] < b.start_coordinates[dimension_index];
                }
                return false;
            });
    }

    /**
     * @brief Sort regions by a specific attribute (numeric).
     * @param attr_name The attribute name.
     */
    void sort_by_attribute(const std::string& attr_name)
    {
        std::sort(regions.begin(), regions.end(),
            [&attr_name](const Region& a, const Region& b) {
                auto a_val = a.get_attribute<double>(attr_name);
                auto b_val = b.get_attribute<double>(attr_name);
                if (a_val && b_val)
                    return *a_val < *b_val;
                return false;
            });
    }

    /**
     * @brief Find all regions with a given label.
     * @param label The label to search for.
     * @return Vector of matching Regions.
     */
    std::vector<Region> find_regions_with_label(const std::string& label) const
    {
        std::vector<Region> result;
        for (const auto& region : regions) {
            if (region.get_label() == label) {
                result.push_back(region);
            }
        }
        return result;
    }

    /**
     * @brief Get the bounding region that contains all regions in the group.
     * @return Region representing the bounding box.
     */
    Region get_bounding_region() const
    {
        if (regions.empty())
            return Region {};

        auto first = regions[0];
        std::vector<u_int64_t> min_coords = first.start_coordinates;
        std::vector<u_int64_t> max_coords = first.end_coordinates;

        for (const auto& region : regions) {
            for (size_t i = 0; i < min_coords.size(); i++) {
                if (i < region.start_coordinates.size()) {
                    min_coords[i] = std::min(min_coords[i], region.start_coordinates[i]);
                    max_coords[i] = std::max(max_coords[i], region.end_coordinates[i]);
                }
            }
        }

        Region bounds(min_coords, max_coords);
        bounds.set_attribute("type", "bounding_box");
        bounds.set_attribute("source_group", name);
        return bounds;
    }

    /**
     * @brief Find all regions with a specific attribute value.
     * @param key Attribute key.
     * @param value Attribute value to match.
     * @return Vector of matching Regions.
     */
    std::vector<Region> find_regions_with_attribute(const std::string& key, const std::any& value) const
    {
        std::vector<Region> result;
        for (const auto& region : regions) {
            auto it = region.attributes.find(key);
            if (it != region.attributes.end()) {
                try {
                    if (it->second.type() == value.type()) {
                        if (value.type() == typeid(std::string)) {
                            if (std::any_cast<std::string>(it->second) == std::any_cast<std::string>(value)) {
                                result.push_back(region);
                            }
                        } else if (value.type() == typeid(double)) {
                            if (std::any_cast<double>(it->second) == std::any_cast<double>(value)) {
                                result.push_back(region);
                            }
                        } else if (value.type() == typeid(int)) {
                            if (std::any_cast<int>(it->second) == std::any_cast<int>(value)) {
                                result.push_back(region);
                            }
                        } else if (value.type() == typeid(bool)) {
                            if (std::any_cast<bool>(it->second) == std::any_cast<bool>(value)) {
                                result.push_back(region);
                            }
                        }
                    }
                } catch (const std::bad_any_cast&) {
                }
            }
        }
        return result;
    }

    /**
     * @brief Find all regions containing the given coordinates.
     * @param coordinates N-dimensional coordinates.
     * @return Vector of matching Regions.
     */
    std::vector<Region> find_regions_containing_coordinates(const std::vector<u_int64_t>& coordinates) const
    {
        std::vector<Region> result;
        for (const auto& region : regions) {
            if (region.contains(coordinates)) {
                result.push_back(region);
            }
        }
        return result;
    }

    /**
     * @brief Set a group-level attribute.
     * @tparam T Value type.
     * @param key Attribute key.
     * @param value Value to set.
     */
    template <typename T>
    void set_attribute(const std::string& key, T value)
    {
        attributes[key] = std::move(value);
    }

    /**
     * @brief Get a group-level attribute.
     * @tparam T Value type.
     * @param key Attribute key.
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
};

} // namespace MayaFlux::Kakshya
