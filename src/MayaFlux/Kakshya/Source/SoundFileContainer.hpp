#pragma once

#include "MayaFlux/Kakshya/FileContainer.hpp"
#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SoundFileContainer
 * @brief Clean N-dimensional audio file container focused on data storage
 *
 * This container stores audio file data as N-dimensional arrays without making
 * musical assumptions. It delegates all processing logic to DataProcessor classes
 * and focuses purely on efficient data access and basic state management.
 *
 * Dimensions:
 * - [0] Time (samples/frames)
 * - [1] Channels
 * - [N] Additional dimensions for spectral data, analysis results, etc.
 */
class SoundFileContainer : public FileContainer {
public:
    SoundFileContainer();
    ~SoundFileContainer() = default;

    // ===== NDDataContainer Implementation =====
    std::vector<DataDimension> get_dimensions() const override;
    u_int64_t get_total_elements() const override;
    MemoryLayout get_memory_layout() const override { return m_memory_layout; }
    void set_memory_layout(MemoryLayout layout) override;

    u_int64_t get_frame_size() const override;
    u_int64_t get_num_frames() const override;

    DataVariant get_region_data(const Region& region) const override;
    void set_region_data(const Region& region, const DataVariant& data) override;

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

    // ===== Region Management =====
    void add_region_group(const RegionGroup& group) override;
    const RegionGroup& get_region_group(const std::string& name) const override;
    std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    bool is_region_loaded(const Region& region) const override;
    void load_region(const Region& region) override;
    void unload_region(const Region& region) override;

    // ===== StreamContainer Implementation =====
    void set_read_position(u_int64_t position) override;
    u_int64_t get_read_position() const override;
    void advance_read_position(u_int64_t frames) override;
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
    u_int64_t get_remaining_frames() const override;
    u_int64_t read_sequential(std::span<double> output, u_int64_t count) override;
    u_int64_t peek_sequential(std::span<double> output, u_int64_t count, u_int64_t offset = 0) const override;

    // ===== SignalSourceContainer Implementation =====
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

    void clear_all_consumption();

    DataVariant& get_processed_data() override { return m_processed_data; }
    const DataVariant& get_processed_data() const override { return m_processed_data; }

    void mark_buffers_for_processing(bool should_process) override { /* Delegate to buffer integration */ }
    void mark_buffers_for_removal() override { /* Delegate to buffer integration */ }

    // ===== Data Setup (for IO classes) =====
    void setup(u_int64_t num_frames, u_int32_t sample_rate, u_int32_t num_channels);
    void set_raw_data(const DataVariant& data);
    void set_all_raw_data(const DataVariant& data);

    // ===== Audio Metadata =====
    u_int32_t get_sample_rate() const { return m_sample_rate; }
    u_int32_t get_num_channels() const { return m_num_channels; }
    double get_duration_seconds() const;

    inline std::span<const double> get_data_as_double() const
    {
        return get_typed_data<double>(m_data);
    }

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

private:
    // ===== Core Data =====
    DataVariant m_data; // Raw audio data
    DataVariant m_processed_data; // Processed data output

    std::atomic<int> m_processing_token_channel { -1 };

    // ===== Dimensions =====
    std::vector<DataDimension> m_dimensions;
    MemoryLayout m_memory_layout = MemoryLayout::ROW_MAJOR;

    // ===== Audio Properties =====
    u_int32_t m_sample_rate = 48000;
    u_int32_t m_num_channels = 0;
    u_int64_t m_num_frames = 0;

    // ===== Stream State =====
    std::atomic<u_int64_t> m_read_position { 0 };
    bool m_looping_enabled = false;
    Region m_loop_region;

    // ===== Processing State =====
    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;

    // ===== Region Management =====
    std::unordered_map<std::string, RegionGroup> m_region_groups;

    // ===== Reader Tracking =====
    std::unordered_map<u_int32_t, int> m_active_readers;
    std::unordered_set<u_int32_t> m_consumed_dimensions;

    std::unordered_map<u_int32_t, std::unordered_set<u_int32_t>> m_reader_consumed_dimensions;
    std::unordered_map<u_int32_t, u_int32_t> m_dimension_to_next_reader_id;

    // ===== Callbacks =====
    std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> m_state_callback;

    // ===== Thread Safety =====
    mutable std::shared_mutex m_data_mutex;
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_reader_mutex;

    // ===== Helper Methods =====
    void setup_dimensions();
    void notify_state_change(ProcessingState new_state);
    void reorganize_data_layout(MemoryLayout new_layout);
};

} // namespace MayaFlux::Kakshya
