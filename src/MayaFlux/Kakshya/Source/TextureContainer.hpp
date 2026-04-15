#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Kakshya {

/**
 * @class TextureContainer
 * @brief SignalSourceContainer wrapping GPU texture data as addressable pixel bytes.
 *
 * TextureContainer is the Kakshya-layer representation of a 2D texture.
 * It owns a flat interleaved RGBA/float pixel buffer matching the declared
 * ImageFormat, described with IMAGE_COLOR or IMAGE_2D dimensions.
 *
 * The container is the data carrier for Yantra::Texture workflow pipelines.
 * GPU materialization is intentionally not automatic: callers drive upload
 * and download explicitly via from_image() and to_image(), keeping the
 * container decoupled from any specific VKImage lifetime.
 *
 * Dimension convention (IMAGE_COLOR, INTERLEAVED):
 *   dims[0] -> SPATIAL_Y (height)
 *   dims[1] -> SPATIAL_X (width)
 *   dims[2] -> CHANNEL   (bytes per pixel for format)
 *
 * Processing model:
 *   No default processor is created. The container holds bytes passively.
 *   Workflow operations receive and return TextureContainer instances;
 *   TextureExecutionContext dispatches compute shaders against them.
 */
class MAYAFLUX_API TextureContainer : public SignalSourceContainer {
public:
    /**
     * @brief Construct an empty container with declared dimensions.
     * @param width   Texture width in pixels.
     * @param height  Texture height in pixels.
     * @param format  Pixel format; determines bytes per pixel.
     * @param layers   Number of array layers (default 1). set when using with array textures.
     *
     * Allocates a zeroed pixel buffer. No GPU resource is created.
     */
    TextureContainer(uint32_t width, uint32_t height, Portal::Graphics::ImageFormat format, uint32_t layers = 1);

    /**
     * @brief Construct from an existing VKImage, downloading its pixel data.
     * @param image  Source GPU texture. Must be initialized.
     * @param format Pixel format of the image.
     *
     * Performs a blocking GPU->CPU download via TextureLoom.
     * Equivalent to constructing empty then calling from_image().
     */
    TextureContainer(
        const std::shared_ptr<Core::VKImage>& image,
        Portal::Graphics::ImageFormat format);

    ~TextureContainer() override = default;

    TextureContainer(const TextureContainer&) = delete;
    TextureContainer& operator=(const TextureContainer&) = delete;
    TextureContainer(TextureContainer&&) = delete;
    TextureContainer& operator=(TextureContainer&&) = delete;

    [[nodiscard]] uint32_t get_layer_count() const { return static_cast<uint32_t>(m_data.size()); }

    //=========================================================================
    // GPU bridge
    //=========================================================================

    /**
     * @brief Download pixel data from a VKImage into this container.
     * @param image Source GPU texture. Must be initialized and match dimensions.
     * @param layer Array layer index for array textures (default 0).
     *
     * Blocking. Overwrites the existing pixel buffer. Does not change
     * declared width, height, or format — caller must ensure consistency.
     */
    void from_image(const std::shared_ptr<Core::VKImage>& image, uint32_t layer = 0);

    /**
     * @brief Upload the pixel buffer to a new VKImage via TextureLoom.
     * @param layer Array layer index for array textures (default 0).
     * @return Newly created and uploaded VKImage. Null on failure.
     *
     * Creates a fresh 2D texture each call. Does not cache the result.
     * The returned image is owned by TextureLoom's internal registry.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> to_image(uint32_t layer = 0) const;

    //=========================================================================
    // Pixel access
    //=========================================================================

    /**
     * @brief Direct read-only span over the raw pixel bytes.
     * @param layer Array layer index for array textures (default 0).
     * @return Contiguous span over the flat interleaved pixel buffer.
     */
    [[nodiscard]] std::span<const uint8_t> pixel_bytes(uint32_t layer = 0) const;

    /**
     * @brief Direct read-write span over the raw pixel bytes.
     * @param layer Array layer index for array textures (default 0).
     * @return Contiguous span over the flat interleaved pixel buffer.
     */
    [[nodiscard]] std::span<uint8_t> pixel_bytes(uint32_t layer = 0);

    /**
     * @brief Replace the pixel buffer contents.
     * @param layer Array layer index for array textures (default 0).
     * @param data Source bytes. Size must equal width * height * bytes_per_pixel.
     */
    void set_pixels(std::span<const uint8_t> data, uint32_t layer = 0);

    //=========================================================================
    // Metadata
    //=========================================================================

    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }
    [[nodiscard]] Portal::Graphics::ImageFormat get_format() const { return m_format; }
    [[nodiscard]] uint32_t get_channel_count() const { return m_channels; }

    /** @brief Byte count of one complete pixel row. */
    [[nodiscard]] size_t row_stride() const { return static_cast<size_t>(m_width) * m_bpp; }

    /** @brief Total byte count of the pixel buffer. */
    [[nodiscard]] size_t byte_size() const { return static_cast<size_t>(m_width) * m_height * m_bpp; }

    //=========================================================================
    // NDDimensionalContainer
    //=========================================================================

    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;
    [[nodiscard]] uint64_t get_total_elements() const override;
    [[nodiscard]] MemoryLayout get_memory_layout() const override;
    void set_memory_layout(MemoryLayout layout) override;
    [[nodiscard]] uint64_t get_frame_size() const override;
    [[nodiscard]] uint64_t get_num_frames() const override;

    [[nodiscard]] std::vector<DataVariant> get_region_data(const Region& region) const override;
    [[nodiscard]] std::vector<DataVariant> get_segments_data(
        const std::vector<RegionSegment>& segments) const override;
    [[nodiscard]] std::vector<DataVariant> get_region_group_data(const RegionGroup&) const override { return m_processed_data; }

    [[nodiscard]] double get_value_at(const std::vector<uint64_t>& coordinates) const override;
    void set_value_at(const std::vector<uint64_t>& coordinates, double value) override;
    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coords) const override;
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t index) const override;
    void clear() override;

    //=========================================================================
    // SignalSourceContainer
    //=========================================================================

    /**
     * @brief Returns a normalized double view of one pixel row.
     *
     * Maps frame_index to row index (SPATIAL_Y). Each uint8_t sample is
     * normalized to [0.0, 1.0]. The returned span is backed by m_frame_cache
     * and remains valid until the next call to get_frame() or clear().
     *
     * @param frame_index Row index in [0, height).
     * @return Span of width * channels doubles, or empty if out of range.
     */
    [[nodiscard]] std::span<const double> get_frame(uint64_t frame_index) const override;

    /**
     * @brief Copy a range of pixel rows into output as normalized doubles.
     *
     * Rows [start_frame, start_frame + num_frames) are written sequentially.
     * Each uint8_t sample is normalized to [0.0, 1.0]. Rows beyond the image
     * height are skipped silently.
     *
     * @param output      Destination buffer. Must hold at least
     *                    num_frames * width * channels doubles.
     * @param start_frame First row index.
     * @param num_frames  Number of rows to copy.
     */
    void get_frames(std::span<double> output,
        uint64_t start_frame, uint64_t num_frames) const override;

    /**
     * @brief Write DataVariant bytes into the pixel buffer at the region bounds.
     *
     * The region's SPATIAL_Y / SPATIAL_X coordinates are used as the destination
     * rectangle. The first DataVariant in @p data must hold std::vector<uint8_t>
     * with exactly region_width * region_height * channels bytes.
     *
     * @param region Destination region (start/end SPATIAL_Y, SPATIAL_X coords).
     * @param data   Source data. First element must be vector<uint8_t>.
     */
    void set_region_data(const Region& region,
        const std::vector<DataVariant>& data) override;

    [[nodiscard]] ProcessingState get_processing_state() const override;
    void update_processing_state(ProcessingState state) override;
    void register_state_change_callback(
        std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> cb) override;
    void unregister_state_change_callback() override;

    [[nodiscard]] bool is_ready_for_processing() const override;
    void mark_ready_for_processing(bool ready) override;

    [[nodiscard]] std::vector<DataVariant>& get_processed_data() override;
    [[nodiscard]] const std::vector<DataVariant>& get_processed_data() const override;
    [[nodiscard]] const std::vector<DataVariant>& get_data() override;

    /** @brief No-op. TextureContainer has no BufferManager integration. */
    void mark_buffers_for_processing(bool) override { }

    /** @brief No-op. TextureContainer has no BufferManager integration. */
    void mark_buffers_for_removal() override { }

    [[nodiscard]] DataAccess channel_data(size_t channel_index) override;
    [[nodiscard]] std::vector<DataAccess> all_channel_data() override;

    /** @brief No-op. All pixel data is always resident; no streaming regions. */
    void load_region(const Region&) override { }

    /** @brief No-op. All pixel data is always resident; no streaming regions. */
    void unload_region(const Region&) override { }
    [[nodiscard]] bool is_region_loaded(const Region&) const override { return true; }

    void add_region_group(const RegionGroup& group) override;
    [[nodiscard]] const RegionGroup& get_region_group(const std::string& name) const override;
    [[nodiscard]] std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    void lock() override;
    void unlock() override;
    [[nodiscard]] bool try_lock() override;

    [[nodiscard]] const void* get_raw_data() const override;
    [[nodiscard]] bool has_data() const override;

    [[nodiscard]] ContainerDataStructure& get_structure() override { return m_structure; }
    [[nodiscard]] const ContainerDataStructure& get_structure() const override { return m_structure; }
    void set_structure(ContainerDataStructure s) override { m_structure = std::move(s); }

    void reset_processing_token() { m_processing_token.store(-1); }
    bool try_acquire_processing_token(int ch)
    {
        int expected = -1;
        return m_processing_token.compare_exchange_strong(expected, ch);
    }
    [[nodiscard]] bool has_processing_token(int ch) const
    {
        return m_processing_token.load() == ch;
    }

    /** @brief No-op. TextureContainer holds bytes passively; no processor lifecycle. */
    void create_default_processor() override { }

    /** @brief No-op. TextureContainer holds bytes passively; no processor lifecycle. */
    void process_default() override { }

    void set_default_processor(const std::shared_ptr<DataProcessor>& p) override { m_processor = p; }
    std::shared_ptr<DataProcessor> get_default_processor() const override { return m_processor; }
    std::shared_ptr<DataProcessingChain> get_processing_chain() override { return m_chain; }
    void set_processing_chain(const std::shared_ptr<DataProcessingChain>& c) override { m_chain = c; }

    /**
     * @brief No-op. TextureContainer does not track concurrent dimension readers.
     * @return Always 0.
     */
    uint32_t register_dimension_reader(uint32_t) override { return 0; }

    /** @brief No-op. TextureContainer does not track concurrent dimension readers. */
    void unregister_dimension_reader(uint32_t) override { }

    /**
     * @brief No-op. TextureContainer does not track concurrent dimension readers.
     * @return Always false.
     */
    [[nodiscard]] bool has_active_readers() const override { return false; }

    /** @brief No-op. TextureContainer does not track concurrent dimension readers. */
    void mark_dimension_consumed(uint32_t, uint32_t) override { }

    /**
     * @brief No-op. TextureContainer does not track concurrent dimension readers.
     * @return Always true.
     */
    [[nodiscard]] bool all_dimensions_consumed() const override { return true; }

private:
    void setup_dimensions();

    uint32_t m_width {};
    uint32_t m_height {};
    Portal::Graphics::ImageFormat m_format {};
    uint32_t m_channels {};
    size_t m_bpp {};

    std::vector<DataVariant> m_data;
    std::vector<DataVariant> m_processed_data;
    std::shared_ptr<DataProcessor> m_processor;
    std::shared_ptr<DataProcessingChain> m_chain;

    ContainerDataStructure m_structure;

    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::atomic<bool> m_ready_for_processing { false };
    std::atomic<int> m_processing_token { -1 };

    mutable std::shared_mutex m_data_mutex;
    std::mutex m_state_mutex;

    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> m_state_cb;
    std::unordered_map<std::string, RegionGroup> m_region_groups;

    /** @brief Backing storage for per-channel DataVariants returned by channel_data(). */
    mutable std::vector<DataVariant> m_channel_cache;

    /** @brief Row cache backing the double span returned by get_frame(). */
    mutable std::vector<double> m_frame_cache;
};

} // namespace MayaFlux::Kakshya
