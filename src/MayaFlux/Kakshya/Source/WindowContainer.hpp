#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Kakshya {

/**
 * @class WindowContainer
 * @brief SignalSourceContainer wrapping a live GLFW/Vulkan window surface.
 *
 * Exposes a window's rendered surface as addressable N-dimensional data.
 * Dimensions follow IMAGE_COLOR convention [height, width, channels].
 *
 * Processing model:
 *   - process() performs one full-surface GPU readback into processed_data[0].
 *   - Region selection is CPU-side: get_region_data() crops from processed_data[0].
 *   - load_region_tracked() registers a rect and returns a stable slot index
 *     for consumer tracking across frames.
 *   - GPU work is constant per frame regardless of region count.
 *
 * The default processor (WindowAccessProcessor) guarantees that on each
 * process() call, get_processed_data() returns one interleaved
 * std::vector<uint8_t> (RGBA8) per loaded region, in load order.
 *
 * Region semantics:
 *   load_region_tracked()     — registers a pixel rect as active; processor reads it, returns its index.
 *   unload_region()   — removes the rect; processor stops reading it.
 *   is_region_loaded()— returns true if the rect intersects the active set.
 *
 * Write semantics (region replacement/compositing) are pinned for a
 * future processor and do not affect this interface.
 */
class MAYAFLUX_API WindowContainer : public SignalSourceContainer {
public:
    /**
     * @brief Construct from an existing managed window.
     * @param window Live window whose surface will be addressed as NDData.
     */
    explicit WindowContainer(std::shared_ptr<Core::Window> window);

    ~WindowContainer() override = default;

    WindowContainer(const WindowContainer&) = delete;
    WindowContainer& operator=(const WindowContainer&) = delete;
    WindowContainer(WindowContainer&&) = delete;
    WindowContainer& operator=(WindowContainer&&) = delete;

    /**
     * @brief The underlying window.
     */
    [[nodiscard]] std::shared_ptr<Core::Window> get_window() const { return m_window; }

    // =========================================================================
    // NDDimensionalContainer
    // =========================================================================

    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;
    [[nodiscard]] uint64_t get_total_elements() const override;
    [[nodiscard]] MemoryLayout get_memory_layout() const override;
    void set_memory_layout(MemoryLayout layout) override;

    /**
     * @brief Crop and return pixel data for all loaded regions that intersect
     *        the query region. Each match yields one DataVariant entry
     *        (interleaved uint8_t, row-major, IMAGE_COLOR).
     *        Crops from the last full-surface readback — no GPU work.
     */
    [[nodiscard]] std::vector<DataVariant> get_region_data(const Region& region) const override;
    void set_region_data(const Region& region, const std::vector<DataVariant>& data) override;

    [[nodiscard]] std::vector<DataVariant> get_region_group_data(const RegionGroup& group) const override;
    [[nodiscard]] std::vector<DataVariant> get_segments_data(const std::vector<RegionSegment>& segments) const override;

    [[nodiscard]] double get_value_at(const std::vector<uint64_t>& coordinates) const override;
    void set_value_at(const std::vector<uint64_t>& coordinates, double value) override;

    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override;
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override;

    void clear() override;
    void lock() override;
    void unlock() override;
    bool try_lock() override;

    [[nodiscard]] const void* get_raw_data() const override;
    [[nodiscard]] bool has_data() const override;

    [[nodiscard]] ContainerDataStructure& get_structure() override { return m_structure; }
    [[nodiscard]] const ContainerDataStructure& get_structure() const override { return m_structure; }
    void set_structure(ContainerDataStructure s) override { m_structure = std::move(s); }

    void add_region_group(const RegionGroup& group) override;
    [[nodiscard]] const RegionGroup& get_region_group(const std::string& name) const override;
    [[nodiscard]] std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    /**
     * @brief Register a pixel rect for CPU-side cropping. Thin wrapper around
     *        load_region_tracked(); discards the returned slot index.
     */
    void load_region(const Region& region) override;

    /**
     * @brief Register a pixel rect and return its stable slot index.
     *        Returns the existing slot index if the region is already loaded.
     *        The slot index is the handle for consumer tracking via
     *        register_dimension_reader() / mark_dimension_consumed().
     */
    std::optional<uint32_t> load_region_tracked(const Region& region);

    /**
     * @brief Deregister a pixel rectangle; processor stops reading it next cycle.
     */
    void unload_region(const Region& region) override;

    /**
     * @brief Returns true if any loaded region spatially intersects the query.
     */
    [[nodiscard]] bool is_region_loaded(const Region& region) const override;

    /**
     * @brief Read-only access to the current loaded-region set.
     *        Used by WindowAccessProcessor to iterate readback targets.
     */
    [[nodiscard]] const std::vector<Region>& get_loaded_regions() const;

    // =========================================================================
    // SignalSourceContainer
    // =========================================================================

    [[nodiscard]] ProcessingState get_processing_state() const override;
    void update_processing_state(ProcessingState new_state) override;

    void register_state_change_callback(
        std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> callback) override;
    void unregister_state_change_callback() override;

    [[nodiscard]] bool is_ready_for_processing() const override;
    void mark_ready_for_processing(bool ready) override;

    void create_default_processor() override;
    void process_default() override;
    void set_default_processor(const std::shared_ptr<DataProcessor>& processor) override;
    [[nodiscard]] std::shared_ptr<DataProcessor> get_default_processor() const override;

    [[nodiscard]] std::shared_ptr<DataProcessingChain> get_processing_chain() override;
    void set_processing_chain(const std::shared_ptr<DataProcessingChain>& chain) override;

    // -------------------------------------------------------------------------
    // Consumer tracking — dimension_index and reader_id are opaque slot handles.
    // Tracks whether all registered consumers have read processed_data[0] this
    // cycle. The slot index argument exists for interface compliance; internally
    // this container tracks a single consumer count across all slots.
    // -------------------------------------------------------------------------
    uint32_t register_dimension_reader(uint32_t dimension_index) override;
    void unregister_dimension_reader(uint32_t dimension_index) override;
    [[nodiscard]] bool has_active_readers() const override;
    void mark_dimension_consumed(uint32_t dimension_index, uint32_t reader_id) override;
    [[nodiscard]] bool all_dimensions_consumed() const override;

    [[nodiscard]] std::vector<DataVariant>& get_processed_data() override;
    [[nodiscard]] const std::vector<DataVariant>& get_processed_data() const override;
    [[nodiscard]] const std::vector<DataVariant>& get_data() override;

    void mark_buffers_for_processing(bool) override { }
    void mark_buffers_for_removal() override { }

    [[nodiscard]] DataAccess channel_data(size_t channel_index) override;
    [[nodiscard]] std::vector<DataAccess> all_channel_data() override;

private:
    std::shared_ptr<Core::Window> m_window;

    ContainerDataStructure m_structure;
    std::vector<DataVariant> m_data;
    std::vector<DataVariant> m_processed_data;

    std::vector<Region> m_loaded_regions;

    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;

    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::atomic<bool> m_ready_for_processing { false };

    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> m_state_callback;
    std::unordered_map<std::string, RegionGroup> m_region_groups;

    mutable std::shared_mutex m_data_mutex;
    mutable std::mutex m_state_mutex;

    std::atomic<uint32_t> m_registered_readers { 0 };
    std::atomic<uint32_t> m_consumed_readers { 0 };
    std::atomic<uint32_t> m_next_reader_id { 0 };

    void setup_dimensions();

    /**
     * @brief CPU-side crop of a pixel rect from the full-surface readback.
     * @param src    Full-surface interleaved pixel data.
     * @param x0     Left edge of the crop rect.
     * @param y0     Top edge of the crop rect.
     * @param pw     Width of the crop rect in pixels.
     * @param ph     Height of the crop rect in pixels.
     * @return Interleaved uint8_t blob of size pw * ph * m_channels.
     */
    [[nodiscard]] std::vector<uint8_t> crop_region(
        const std::vector<uint8_t>& src,
        uint32_t x0, uint32_t y0,
        uint32_t pw, uint32_t ph) const;

    /**
     * @brief Returns true if r1 and r2 spatially overlap on the Y/X axes.
     */
    [[nodiscard]] static bool regions_intersect(const Region& r1, const Region& r2) noexcept;
};

} // namespace MayaFlux::Kakshya
