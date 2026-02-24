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
 * Dimensions follow IMAGE_COLOR convention:
 *   dims[0] → SPATIAL_Y (height)
 *   dims[1] → SPATIAL_X (width)
 *   dims[2] → CHANNEL
 *
 * Region semantics:
 *   Regions are registered through the inherited RegionGroup API
 *   (add_region_group / get_all_region_groups / remove_region_group).
 *   load_region() and unload_region() are no-ops on this container — all
 *   surface data is always available after a GPU readback; region selection
 *   is a processor concern, not a container concern.
 *
 * Processing model:
 *   - The default processor (WindowAccessProcessor) performs one full-surface
 *     GPU readback per process() call into processed_data[0].
 *   - The processing chain (SpatialRegionProcessor) extracts registered
 *     regions from the readback as separate DataVariant entries.
 *   - get_region_data() crops directly from processed_data[0] if it is
 *     non-empty; returns empty if no readback has occurred yet.
 *   - The container never calls process() itself — callers drive the chain.
 *
 * Write semantics (compositing) are deferred to a future processor.
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
     * @brief Extract data for all regions across all region groups that
     *        spatially intersect @p region.
     *        Crops from the last full-surface readback — no GPU work.
     *        Returns empty if no readback has been performed yet.
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

    // -------------------------------------------------------------------------
    // RegionGroup API — primary region registration interface.
    // -------------------------------------------------------------------------

    void add_region_group(const RegionGroup& group) override;
    [[nodiscard]] const RegionGroup& get_region_group(const std::string& name) const override;
    [[nodiscard]] std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    /**
     * @brief No-op. All surface data is continuously available after readback.
     *        Register regions via add_region_group() instead.
     */
    void load_region(const Region& region) override;

    /**
     * @brief No-op. See load_region().
     */
    void unload_region(const Region& region) override;

    /**
     * @brief Always returns true. Surface data is available after any readback.
     */
    [[nodiscard]] bool is_region_loaded(const Region& region) const override;

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

    std::unordered_map<std::string, RegionGroup> m_region_groups;

    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;

    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::atomic<bool> m_ready_for_processing { false };

    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> m_state_callback;

    mutable std::shared_mutex m_data_mutex;
    mutable std::mutex m_state_mutex;

    std::atomic<uint32_t> m_registered_readers { 0 };
    std::atomic<uint32_t> m_consumed_readers { 0 };
    std::atomic<uint32_t> m_next_reader_id { 0 };

    void setup_dimensions();
};

} // namespace MayaFlux::Kakshya
