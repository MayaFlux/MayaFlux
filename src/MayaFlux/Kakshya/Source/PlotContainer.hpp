#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Nodes {
class Node;
namespace Network {
    class NodeNetwork;
}
}

namespace MayaFlux::Buffers {
class AudioBuffer;
}

namespace MayaFlux::Kakshya {

class PlotProcessor;

/**
 * @class PlotContainer
 * @brief SignalSourceContainer holding N named scalar series for plotting and signal use.
 *
 * Each series is a named std::vector<double> stored as a DataVariant in m_data,
 * with a corresponding DataDimension (Role::CUSTOM, size = sample count, name = series name).
 * processed_data mirrors m_data after PlotProcessor::process() — one DataVariant per series,
 * index-stable, suitable for direct consumption by Forma geometry functions.
 *
 * Series are append-only by index. Resize via resize_series() when the sample count changes.
 * Region semantics apply per-series: a Region with start/end coordinates on the TIME-equivalent
 * axis (dim 0) selects a sample range within that series. get_region_data() returns the
 * selected slice as vector<double>, enabling wavetable read-back and drag-to-select interaction.
 *
 * Dimension tracking, frame/stream navigation, and processing-token machinery inherited from
 * SignalSourceContainer are no-op stubs here, grown into as needed.
 */
class MAYAFLUX_API PlotContainer : public SignalSourceContainer {
public:
    /**
     * @brief Construct an empty container. Series are added via add_series().
     */
    PlotContainer();

    ~PlotContainer() override = default;

    PlotContainer(const PlotContainer&) = delete;
    PlotContainer& operator=(const PlotContainer&) = delete;
    PlotContainer(PlotContainer&&) = delete;
    PlotContainer& operator=(PlotContainer&&) = delete;

    // =========================================================================
    // Series management
    // =========================================================================

    /**
     * @brief Add a named series with a given capacity, zero-initialised.
     * @param name   Series name. Used as the DataDimension name and for lookup.
     * @param count  Number of samples. Sets the DataDimension size.
     * @return Index of the newly added series.
     */
    uint32_t add_series(std::string name, uint64_t count);

    // =========================================================================
    // Source binding — delegates to PlotProcessor, creating it if absent.
    //
    // Each bind_* call associates a data source with a series slot. The
    // processor acquires from that source on every process() call and writes
    // into m_data. Binding a slot that already has a source replaces it.
    // =========================================================================

    /**
     * @brief Bind a series to a Node.
     *
     * Each process() fills the series by calling
     * Buffers::extract_multiple_samples(node, series_size), which handles
     * snapshot context and node lifecycle identically to NodeSourceProcessor.
     *
     * @param series_index  Index returned by add_series().
     * @param node          Node to read from.
     */
    void bind_node(uint32_t series_index, std::shared_ptr<Nodes::Node> node);

    /**
     * @brief Bind a series to an AudioBuffer.
     *
     * Each process() copies get_data() span into the series.
     * Series size is not automatically resized; allocate via add_series()
     * to match the expected buffer frame count.
     *
     * @param series_index  Index returned by add_series().
     * @param buffer        AudioBuffer to read from.
     */
    void bind_audio_buffer(uint32_t series_index,
        std::shared_ptr<Buffers::AudioBuffer> buffer);

    /**
     * @brief Bind a series to a NodeNetwork with audio output.
     *
     * Each process() reads get_audio_buffer() from the network.
     * Fails at bind time if the network has no audio output mode.
     *
     * @param series_index  Index returned by add_series().
     * @param network       NodeNetwork to read from.
     */
    void bind_network(uint32_t series_index,
        std::shared_ptr<Nodes::Network::NodeNetwork> network);

    /**
     * @brief Bind a series to a callable.
     *
     * Each process() calls fn(series_vector) by reference. The callable
     * fills or mutates it freely. Use for computed series, ring buffer
     * views, or any source that does not fit the other patterns.
     *
     * @param series_index  Index returned by add_series().
     * @param fn            Callable invoked with the series vector each process().
     */
    void bind_callable(uint32_t series_index,
        std::function<void(std::vector<double>&)> fn);

    /**
     * @brief Push raw sample data into a series.
     *
     * Lock-free pending swap committed on the next process() call.
     * May be called from any thread.
     *
     * @param series_index  Index returned by add_series().
     * @param data          Samples to write. Size should match series capacity.
     */
    void set_raw(uint32_t series_index, std::vector<double> data);

    /**
     * @brief Remove the source binding for a series.
     * @param series_index Series to unbind.
     */
    void unbind(uint32_t series_index);

    /**
     * @brief Write the full sample buffer for a series.
     * @param index   Series index from add_series().
     * @param samples Source data. Must match the series sample count.
     */
    void write_series(uint32_t index, std::span<const double> samples);

    /**
     * @brief Write a single sample within a series.
     * @param index        Series index.
     * @param sample_index Sample position within the series.
     * @param value        Value to write.
     */
    void write_sample(uint32_t index, uint64_t sample_index, double value);

    /**
     * @brief Resize a series. Truncates or zero-extends. Updates the DataDimension.
     * @param index Series index.
     * @param count New sample count.
     */
    void resize_series(uint32_t index, uint64_t count);

    /**
     * @brief Return the number of series.
     */
    [[nodiscard]] uint32_t series_count() const;

    /**
     * @brief Return the name of a series.
     */
    [[nodiscard]] const std::string& series_name(uint32_t index) const;

    /**
     * @brief Return the sample count of a series.
     */
    [[nodiscard]] uint64_t series_size(uint32_t index) const;

    // =========================================================================
    // NDDataContainer
    // =========================================================================

    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;
    [[nodiscard]] uint64_t get_total_elements() const override;
    [[nodiscard]] MemoryLayout get_memory_layout() const override { return MemoryLayout::ROW_MAJOR; }
    void set_memory_layout(MemoryLayout) override { }

    [[nodiscard]] uint64_t get_frame_size() const override { return 1; }
    [[nodiscard]] uint64_t get_num_frames() const override;

    /**
     * @brief Extract a sample range from a single series.
     *
     * region.start_coordinates[0] = series index.
     * region.start_coordinates[1] = first sample (inclusive).
     * region.end_coordinates[1]   = last sample (inclusive).
     *
     * Returns a single DataVariant containing vector<double> of the slice.
     * Returns empty if coordinates are out of range.
     */
    [[nodiscard]] std::vector<DataVariant> get_region_data(const Region& region) const override;

    /**
     * @brief Write a sample range back into a series.
     *
     * Same coordinate convention as get_region_data(). The first DataVariant
     * in data must hold vector<double>. Used by Context drag handlers for
     * wavetable editing and interactive signal manipulation.
     */
    void set_region_data(const Region& region, const std::vector<DataVariant>& data) override;

    [[nodiscard]] std::vector<DataVariant> get_region_group_data(const RegionGroup& group) const override;
    [[nodiscard]] std::vector<DataVariant> get_segments_data(const std::vector<RegionSegment>& segments) const override;

    [[nodiscard]] std::span<const double> get_frame(uint64_t frame_index) const override;
    void get_frames(std::span<double> output, uint64_t start_frame, uint64_t num_frames) const override;

    [[nodiscard]] double get_value_at(const std::vector<uint64_t>& coordinates) const override;
    void set_value_at(const std::vector<uint64_t>& coordinates, double value) override;

    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override;
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override;

    void clear() override;
    void lock() override { }
    void unlock() override { }
    bool try_lock() override { return true; }

    [[nodiscard]] const void* get_raw_data() const override;
    [[nodiscard]] bool has_data() const override;

    [[nodiscard]] ContainerDataStructure& get_structure() override { return m_structure; }
    [[nodiscard]] const ContainerDataStructure& get_structure() const override { return m_structure; }
    void set_structure(ContainerDataStructure s) override { m_structure = std::move(s); }

    void add_region_group(const RegionGroup& group) override;
    [[nodiscard]] const RegionGroup& get_region_group(const std::string& name) const override;
    [[nodiscard]] std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    [[nodiscard]] bool is_region_loaded(const Region&) const override { return true; }
    void load_region(const Region&) override { }
    void unload_region(const Region&) override { }

    [[nodiscard]] DataAccess channel_data(size_t index) override;
    [[nodiscard]] std::vector<DataAccess> all_channel_data() override;

    // =========================================================================
    // SignalSourceContainer
    // =========================================================================

    [[nodiscard]] ProcessingState get_processing_state() const override;
    void update_processing_state(ProcessingState state) override;

    void register_state_change_callback(
        std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> cb) override;
    void unregister_state_change_callback() override;

    [[nodiscard]] bool is_ready_for_processing() const override;
    void mark_ready_for_processing(bool ready) override;

    void create_default_processor() override;
    void process_default() override;

    void set_default_processor(const std::shared_ptr<DataProcessor>& processor) override;
    [[nodiscard]] std::shared_ptr<DataProcessor> get_default_processor() const override;

    [[nodiscard]] std::shared_ptr<DataProcessingChain> get_processing_chain() override { return m_chain; }
    void set_processing_chain(const std::shared_ptr<DataProcessingChain>& chain) override { m_chain = chain; }

    [[nodiscard]] std::vector<DataVariant>& get_processed_data() override { return m_processed_data; }
    [[nodiscard]] const std::vector<DataVariant>& get_processed_data() const override { return m_processed_data; }
    [[nodiscard]] const std::vector<DataVariant>& get_data() override { return m_data; }

    void mark_buffers_for_processing(bool) override { }
    void mark_buffers_for_removal() override { }

    // ---- dimension reader stubs (no concurrent consumer tracking needed yet) ----
    uint32_t register_dimension_reader(uint32_t) override { return 0; }
    void unregister_dimension_reader(uint32_t) override { }
    [[nodiscard]] bool has_active_readers() const override { return false; }
    void mark_dimension_consumed(uint32_t, uint32_t) override { }
    [[nodiscard]] bool all_dimensions_consumed() const override { return true; }

private:
    void rebuild_dimensions();

    /**
     * @brief Return the PlotProcessor, creating and attaching it if absent.
     *
     * Called by all bind_* methods. Guarantees the processor exists before
     * delegating the bind call, without requiring the caller to manage it.
     */
    PlotProcessor& ensure_processor();

    std::vector<std::string> m_names;
    std::vector<DataVariant> m_data;
    std::vector<DataVariant> m_processed_data;
    std::vector<DataDimension> m_dimensions;

    ContainerDataStructure m_structure;
    std::shared_ptr<DataProcessor> m_processor;
    std::shared_ptr<DataProcessingChain> m_chain;

    std::unordered_map<std::string, RegionGroup> m_region_groups;

    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::atomic<bool> m_ready_for_processing { false };

    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> m_state_cb;

    mutable std::vector<double> m_frame_cache;
};

} // namespace MayaFlux::Kakshya
