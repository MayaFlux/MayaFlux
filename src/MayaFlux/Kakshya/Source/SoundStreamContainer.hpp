#pragma once

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Kakshya/StreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SoundStreamContainer
 * @brief Concrete base implementation for streaming audio containers.
 *
 * SoundStreamContainer provides a complete, concrete implementation of all StreamContainer
 * functionality for audio data. It serves as:
 * 1. A standalone streaming container for real-time audio processing
 * 2. A base class for specialized containers like SoundFileContainer
 *
 * The container implements all common audio streaming operations including:
 * - Region management and processing state tracking
 * - Sequential reading with looping support
 * - Multi-dimensional data access and coordinate mapping
 * - Processing chain integration and reader tracking
 * - Memory layout optimization and data reorganization
 *
 * Uses virtual inheritance to support diamond inheritance when used as a base
 * for FileContainer-derived classes.
 */
class SoundStreamContainer : public virtual StreamContainer {
public:
    /**
     * @brief Construct a SoundStreamContainer with specified parameters.
     * @param sample_rate Sample rate for temporal calculations
     * @param num_channels Number of audio channels
     * @param initial_capacity Initial capacity in frames (0 = minimal allocation)
     * @param circular_mode If true, acts as circular buffer with fixed capacity
     */
    SoundStreamContainer(u_int32_t sample_rate = 48000,
        u_int32_t num_channels = 2,
        u_int64_t initial_capacity = 0,
        bool circular_mode = false);

    ~SoundStreamContainer() override = default;

    std::vector<DataDimension> get_dimensions() const override;
    u_int64_t get_total_elements() const override;
    MemoryLayout get_memory_layout() const override { return m_structure.memory_layout; }
    void set_memory_layout(MemoryLayout layout) override;

    u_int64_t get_frame_size() const override;
    u_int64_t get_num_frames() const override;

    std::vector<DataVariant> get_region_data(const Region& region) const override;
    void set_region_data(const Region& region, const std::vector<DataVariant>& data) override;

    std::span<const double> get_frame(u_int64_t frame_index) const override;
    void get_frames(std::span<double> output, u_int64_t start_frame, u_int64_t num_frames) const override;

    double get_value_at(const std::vector<u_int64_t>& coordinates) const override;
    void set_value_at(const std::vector<u_int64_t>& coordinates, double value) override;

    u_int64_t coordinates_to_linear_index(const std::vector<u_int64_t>& coordinates) const override;
    std::vector<u_int64_t> linear_index_to_coordinates(u_int64_t linear_index) const override;

    void clear() override;
    void lock() override { m_data_mutex.lock(); }
    void unlock() override { m_data_mutex.unlock(); }
    bool try_lock() override { return m_data_mutex.try_lock(); }

    const void* get_raw_data() const override;
    bool has_data() const override;

    inline ContainerDataStructure& get_structure() override { return m_structure; }
    inline const ContainerDataStructure& get_structure() const override { return m_structure; }

    inline void set_structure(ContainerDataStructure structure) override { m_structure = structure; }

    void add_region_group(const RegionGroup& group) override;
    const RegionGroup& get_region_group(const std::string& name) const override;
    std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    bool is_region_loaded(const Region& region) const override;
    void load_region(const Region& region) override;
    void unload_region(const Region& region) override;

    void set_read_position(const std::vector<u_int64_t>& position) override;
    void update_read_position_for_channel(size_t channel, u_int64_t frame) override;
    const std::vector<u_int64_t>& get_read_position() const override;
    void advance_read_position(const std::vector<u_int64_t>& frames) override;
    bool is_at_end() const override;
    void reset_read_position() override;

    u_int64_t get_temporal_rate() const override { return m_sample_rate; }
    u_int64_t time_to_position(double time) const override;
    double position_to_time(u_int64_t position) const override;

    void set_looping(bool enable) override;
    bool is_looping() const override { return m_looping_enabled; }
    void set_loop_region(const Region& region) override;
    Region get_loop_region() const override;

    bool is_ready() const override;
    std::vector<u_int64_t> get_remaining_frames() const override;
    u_int64_t read_sequential(std::span<double> output, u_int64_t count) override;
    u_int64_t peek_sequential(std::span<double> output, u_int64_t count, u_int64_t offset = 0) const override;

    ProcessingState get_processing_state() const override { return m_processing_state.load(); }
    void update_processing_state(ProcessingState new_state) override;

    void register_state_change_callback(
        std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> callback) override
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_state_callback = callback;
    }

    void unregister_state_change_callback() override
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_state_callback = nullptr;
    }

    bool is_ready_for_processing() const override;
    void mark_ready_for_processing(bool ready) override;

    void create_default_processor() override;
    void process_default() override;

    void set_default_processor(std::shared_ptr<DataProcessor> processor) override;
    std::shared_ptr<DataProcessor> get_default_processor() const override;

    std::shared_ptr<DataProcessingChain> get_processing_chain() override { return m_processing_chain; }
    void set_processing_chain(std::shared_ptr<DataProcessingChain> chain) override { m_processing_chain = chain; }

    u_int32_t register_dimension_reader(u_int32_t dimension_index) override;
    void unregister_dimension_reader(u_int32_t dimension_index) override;
    bool has_active_readers() const override;
    void mark_dimension_consumed(u_int32_t dimension_index, u_int32_t reader_id) override;
    bool all_dimensions_consumed() const override;

    virtual void clear_all_consumption();

    inline std::vector<DataVariant>& get_processed_data() override
    {
        return m_processed_data;
    }

    inline const std::vector<DataVariant>& get_processed_data() const override
    {
        return m_processed_data;
    }

    void mark_buffers_for_processing(bool) override { /* Delegate to buffer integration */ }
    void mark_buffers_for_removal() override { /* Delegate to buffer integration */ }

    // void ensure_capacity(u_int64_t required_frames);
    // u_int64_t get_capacity() const;
    // void set_circular_mode(bool enable, std::optional<u_int64_t> fixed_capacity = std::nullopt);
    // bool is_circular_mode() const { return m_circular_mode; }

    // void setup(u_int64_t num_frames, u_int32_t sample_rate, u_int32_t num_channels);
    // void set_raw_data(const DataVariant& data);
    // void set_all_raw_data(const DataVariant& data);

    virtual u_int32_t get_sample_rate() const { return m_sample_rate; }

    inline virtual u_int32_t get_num_channels() const
    {
        return static_cast<u_int32_t>(m_structure.get_channel_count());
    }
    // double get_duration_seconds() const;

    inline void reset_processing_token() override { m_processing_token_channel.store(-1); }

    inline bool try_acquire_processing_token(int channel) override
    {
        int expected = -1;
        return m_processing_token_channel.compare_exchange_strong(expected, channel);
    }

    inline bool has_processing_token(int channel) const override
    {
        return m_processing_token_channel.load() == channel;
    }

    /**
     * @brief Get the audio data as a specific type
     * @return Span of double data for direct access
     */
    std::span<const double> get_data_as_double() const;

    inline const std::vector<DataVariant>& get_data() override { return m_data; }

protected:
    void setup_dimensions();
    void notify_state_change(ProcessingState new_state);
    void reorganize_data_layout(MemoryLayout new_layout);

    std::vector<DataVariant> m_data;
    std::vector<DataVariant> m_processed_data;

    std::atomic<int> m_processing_token_channel { -1 };

    u_int32_t m_sample_rate = 48000;
    u_int32_t m_num_channels {};
    u_int64_t m_num_frames {};

    std::vector<std::atomic<u_int64_t>> m_read_position;
    bool m_looping_enabled = false;
    Region m_loop_region;

    bool m_circular_mode {};
    u_int64_t m_circular_write_position {};

    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;

    std::unordered_map<std::string, RegionGroup> m_region_groups;

    std::unordered_map<u_int32_t, int> m_active_readers;
    std::unordered_set<u_int32_t> m_consumed_dimensions;

    std::unordered_map<u_int32_t, std::unordered_set<u_int32_t>> m_reader_consumed_dimensions;
    std::unordered_map<u_int32_t, u_int32_t> m_dimension_to_next_reader_id;

    std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> m_state_callback;

    mutable std::shared_mutex m_data_mutex;
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_reader_mutex;

    mutable std::vector<double> m_cached_ext_buffer;

    mutable std::atomic<bool> m_double_extraction_dirty { true };
    mutable std::mutex m_extraction_mutex;

    ContainerDataStructure m_structure;

private:
    mutable std::optional<std::vector<std::span<double>>> m_span_cache;
    mutable std::atomic<bool> m_span_cache_dirty { true };

    /** @brief Get the cached spans for each channel, recomputing if dirty */
    const std::vector<std::span<double>>& get_span_cache() const;

    /** @brief Invalidate the span cache when data or layout changes */
    void invalidate_span_cache();
};

} // namespace MayaFlux::Kakshya
