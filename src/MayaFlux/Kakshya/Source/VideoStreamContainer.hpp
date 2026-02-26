#pragma once

#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"
#include "MayaFlux/Kakshya/StreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class VideoStreamContainer
 * @brief Concrete base implementation for streaming video containers.
 *
 * VideoStreamContainer provides a complete, concrete implementation of all
 * StreamContainer functionality for decoded video frame data. It serves as:
 * 1. A standalone streaming container for real-time video processing
 * 2. A base class for specialized containers like VideoFileContainer
 *
 * Data is stored as uint8_t pixels in RGBA interleaved layout (matching
 * Vulkan VK_FORMAT_R8G8B8A8_UNORM and the TextureBuffer pipeline).
 * Each frame is width * height * channels bytes. All frames are stored
 * contiguously in a single DataVariant.
 *
 * Dimensions follow VIDEO_COLOR convention:
 *   dims[0] → TIME (frame count)
 *   dims[1] → SPATIAL_Y (height)
 *   dims[2] → SPATIAL_X (width)
 *   dims[3] → CHANNEL (colour channels, typically 4 for RGBA)
 *
 * Reader model follows WindowContainer's pattern: a simple atomic reader
 * count rather than per-dimension/per-channel tracking. Video frames are
 * atomic spatial units — channel-level access is a processor concern,
 * not a container concern.
 *
 * Uses virtual inheritance to support diamond inheritance when used as a
 * base for FileContainer-derived classes.
 */
class MAYAFLUX_API VideoStreamContainer : public virtual StreamContainer {
public:
    /**
     * @brief Construct a VideoStreamContainer with specified parameters.
     * @param width       Frame width in pixels.
     * @param height      Frame height in pixels.
     * @param channels    Colour channels per pixel (default 4 for RGBA).
     * @param frame_rate  Temporal rate in frames per second.
     */
    VideoStreamContainer(uint32_t width = 0,
        uint32_t height = 0,
        uint32_t channels = 4,
        double frame_rate = 0.0);

    ~VideoStreamContainer() override = default;

    // =========================================================================
    // NDDimensionalContainer
    // =========================================================================

    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;
    [[nodiscard]] uint64_t get_total_elements() const override;
    [[nodiscard]] MemoryLayout get_memory_layout() const override { return m_structure.memory_layout; }
    void set_memory_layout(MemoryLayout layout) override;

    [[nodiscard]] uint64_t get_frame_size() const override;
    [[nodiscard]] uint64_t get_num_frames() const override;

    std::vector<DataVariant> get_region_data(const Region& region) const override;
    void set_region_data(const Region& region, const std::vector<DataVariant>& data) override;

    std::vector<DataVariant> get_region_group_data(const RegionGroup& group) const override;
    std::vector<DataVariant> get_segments_data(const std::vector<RegionSegment>& segment) const override;

    [[nodiscard]] std::span<const double> get_frame(uint64_t frame_index) const override;
    void get_frames(std::span<double> output, uint64_t start_frame, uint64_t num_frames) const override;

    [[nodiscard]] double get_value_at(const std::vector<uint64_t>& coordinates) const override;
    void set_value_at(const std::vector<uint64_t>& coordinates, double value) override;

    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override;
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override;

    void clear() override;
    void lock() override { m_data_mutex.lock(); }
    void unlock() override { m_data_mutex.unlock(); }
    bool try_lock() override { return m_data_mutex.try_lock(); }

    [[nodiscard]] const void* get_raw_data() const override;
    [[nodiscard]] bool has_data() const override;

    ContainerDataStructure& get_structure() override { return m_structure; }
    const ContainerDataStructure& get_structure() const override { return m_structure; }
    void set_structure(ContainerDataStructure structure) override { m_structure = structure; }

    // =========================================================================
    // RegionGroup management
    // =========================================================================

    void add_region_group(const RegionGroup& group) override;
    const RegionGroup& get_region_group(const std::string& name) const override;
    std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    bool is_region_loaded(const Region& region) const override;
    void load_region(const Region& region) override;
    void unload_region(const Region& region) override;

    // =========================================================================
    // Read position and looping
    // =========================================================================

    void set_read_position(const std::vector<uint64_t>& position) override;
    void update_read_position_for_channel(size_t channel, uint64_t frame) override;
    [[nodiscard]] const std::vector<uint64_t>& get_read_position() const override;
    void advance_read_position(const std::vector<uint64_t>& frames) override;
    [[nodiscard]] bool is_at_end() const override;
    void reset_read_position() override;

    [[nodiscard]] uint64_t get_temporal_rate() const override;
    [[nodiscard]] uint64_t time_to_position(double time) const override;
    [[nodiscard]] double position_to_time(uint64_t position) const override;

    void set_looping(bool enable) override;
    [[nodiscard]] bool is_looping() const override { return m_looping_enabled; }
    void set_loop_region(const Region& region) override;
    [[nodiscard]] Region get_loop_region() const override;

    [[nodiscard]] bool is_ready() const override;
    [[nodiscard]] std::vector<uint64_t> get_remaining_frames() const override;
    uint64_t read_sequential(std::span<double> output, uint64_t count) override;
    uint64_t peek_sequential(std::span<double> output, uint64_t count, uint64_t offset) const override;

    // =========================================================================
    // Processing state
    // =========================================================================

    [[nodiscard]] ProcessingState get_processing_state() const override { return m_processing_state.load(); }
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

    std::shared_ptr<DataProcessingChain> get_processing_chain() override { return m_processing_chain; }
    void set_processing_chain(const std::shared_ptr<DataProcessingChain>& chain) override { m_processing_chain = chain; }

    // =========================================================================
    // Reader tracking (WindowContainer-style atomic counting)
    // =========================================================================

    uint32_t register_dimension_reader(uint32_t dimension_index) override;
    void unregister_dimension_reader(uint32_t dimension_index) override;
    [[nodiscard]] bool has_active_readers() const override;
    void mark_dimension_consumed(uint32_t dimension_index, uint32_t reader_id) override;
    [[nodiscard]] bool all_dimensions_consumed() const override;

    // =========================================================================
    // Processing token
    // =========================================================================

    void reset_processing_token() override { m_processing_token_channel.store(-1); }

    bool try_acquire_processing_token(int channel) override
    {
        int expected = -1;
        return m_processing_token_channel.compare_exchange_strong(expected, channel);
    }

    [[nodiscard]] bool has_processing_token(int channel) const override
    {
        return m_processing_token_channel.load() == channel;
    }

    // =========================================================================
    // Data access
    // =========================================================================

    const std::vector<DataVariant>& get_data() override { return m_data; }

    DataAccess channel_data(size_t channel) override;
    std::vector<DataAccess> all_channel_data() override;

    std::vector<DataVariant>& get_processed_data() override { return m_processed_data; }
    const std::vector<DataVariant>& get_processed_data() const override { return m_processed_data; }

    void mark_buffers_for_processing(bool) override { }
    void mark_buffers_for_removal() override { }

    // =========================================================================
    // Video-specific accessors
    // =========================================================================

    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }
    [[nodiscard]] uint32_t get_channels() const { return m_channels; }
    [[nodiscard]] double get_frame_rate() const { return m_frame_rate; }

    /**
     * @brief Get raw pixel data for a single frame as uint8_t span.
     * @param frame_index Zero-based frame index.
     * @return Span of pixel bytes for the frame, empty if out of range.
     */
    [[nodiscard]] std::span<const uint8_t> get_frame_pixels(uint64_t frame_index) const;

    /**
     * @brief Get the total byte size of one frame (width * height * channels).
     */
    [[nodiscard]] size_t get_frame_byte_size() const;

protected:
    void setup_dimensions();
    void notify_state_change(ProcessingState new_state);

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_channels = 4;
    double m_frame_rate = 0.0;
    uint64_t m_num_frames = 0;

    ContainerDataStructure m_structure;

    std::vector<DataVariant> m_data;
    std::vector<DataVariant> m_processed_data;

    mutable std::shared_mutex m_data_mutex;
    mutable std::mutex m_state_mutex;

    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::atomic<int> m_processing_token_channel { -1 };

    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> m_state_callback;
    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;

    std::unordered_map<std::string, RegionGroup> m_region_groups;

    std::atomic<uint64_t> m_read_position { 0 };
    bool m_looping_enabled = false;
    Region m_loop_region;

    std::atomic<uint32_t> m_registered_readers { 0 };
    std::atomic<uint32_t> m_consumed_readers { 0 };
};

} // namespace MayaFlux::Kakshya
