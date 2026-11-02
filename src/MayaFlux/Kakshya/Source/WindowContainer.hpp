#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Transitive/Memory/SeqLock.hpp"

namespace MayaFlux::Core {
class Window;
class VKImage;
}

namespace MayaFlux::Kakshya {

/**
 * @class WindowContainer
 * @brief SignalSourceContainer wrapping a live Vulkan window surface.
 *
 * Exposes a window's rendered surface as addressable N-dimensional data.
 * Dimensions describe one surface, IMAGE_COLOR convention:
 *   dims[0] -> SPATIAL_Y (height)
 *   dims[1] -> SPATIAL_X (width)
 *   dims[2] -> CHANNEL   (elements per pixel; 1 for packed formats)
 *
 * Element type follows the live swapchain format and is never narrowed:
 *   8-bit UNORM/SRGB -> uint8_t
 *   16-bit SFLOAT    -> uint16_t (raw half-float bits)
 *   32-bit SFLOAT    -> float
 *   10-bit packed    -> uint32_t (one packed word per pixel)
 *
 * History:
 *   The container keeps the last frame_capacity captures as a ring of
 *   surfaces in the same element type. get_num_frames() is the number held,
 *   get_frame(i) is the i-th oldest, and image_at() addresses a ring slot
 *   directly. get_total_elements() and get_frame_size() describe one surface.
 *
 * Region semantics:
 *   Regions are registered through the inherited RegionGroup API
 *   (add_region_group / get_all_region_groups / remove_region_group).
 *   Coordinates are [y, x] or [y, x, channel], inclusive. A region with no
 *   channel range covers every channel. load_region() and unload_region()
 *   are no-ops: region selection is a processor concern.
 *
 * Processing model:
 *   - The default processor (WindowAccessProcessor) performs one full-surface
 *     readback per process() call into processed_data[0].
 *   - The processing chain (SpatialRegionProcessor) extracts registered
 *     regions from the readback as separate DataVariant entries.
 *   - has_data() is false and get_region_data() is empty until the first
 *     capture, and again after clear() or a resize.
 *   - The container never calls process() itself; callers drive the chain.
 *
 * GPU bridge:
 *   - to_image() refreshes a container-owned VKImage from processed_data[0].
 *   - image_at() refreshes a container-owned VKImage for a ring slot.
 *   - region_to_image() crops on the CPU and refreshes an image for that region.
 *   - Packed 10-bit data is unpacked to RGBA16F when an image is made; the
 *     container keeps the packed words.
 *   - get_image_format() returns the live swapchain format as a Portal
 *     ImageFormat, suitable for constructing a matching TextureBuffer.
 *   - Concurrent image materialization calls on one container are serialized.
 *
 * Lifetime:
 *   A resize replaces the surface and the history together under the data
 *   seqlock. Spans, raw pointers and float views obtained before a resize or
 *   the next capture are not valid afterwards.
 *
 * Write semantics (compositing) are deferred to a future processor.
 */
class MAYAFLUX_API WindowContainer : public SignalSourceContainer {
public:
    using SurfaceFormat = Core::GraphicsSurfaceInfo::SurfaceFormat;

    /**
     * @brief Construct from an existing managed window.
     *
     * The surface starts as an 8-bit surface of the window's current size.
     * WindowAccessProcessor reshapes it to the live swapchain on attach and on
     * any later change of extent or format.
     *
     * @param window         Live window whose surface will be addressed as NDData.
     * @param frame_capacity Number of captures to retain. Defaults to 60.
     */
    explicit WindowContainer(std::shared_ptr<Core::Window> window,
        uint32_t frame_capacity = 60);

    ~WindowContainer() override = default;

    WindowContainer(const WindowContainer&) = delete;
    WindowContainer& operator=(const WindowContainer&) = delete;
    WindowContainer(WindowContainer&&) = delete;
    WindowContainer& operator=(WindowContainer&&) = delete;

    /**
     * @brief The underlying window.
     */
    [[nodiscard]] std::shared_ptr<Core::Window> get_window() const { return m_window; }

    /**
     * @brief Portal ImageFormat corresponding to the live swapchain surface format.
     *
     * Derived from the actual negotiated swapchain format via DisplayService,
     * not from the WindowCreateInfo declaration. Use this when constructing a
     * TextureBuffer to receive the output of to_image().
     */
    [[nodiscard]] ImageFormat get_image_format() const;

    /**
     * @brief Surface format the stored data currently has.
     *
     * Follows the swapchain once WindowAccessProcessor has reshaped the
     * container, so it can lag get_image_format() by one capture.
     */
    [[nodiscard]] SurfaceFormat get_surface_format() const;

    /**
     * @brief Mutable pointer to the bytes of history slot @p frame_index.
     * @param frame_index Ring slot in [0, frame_capacity).
     * @return Pointer to the slot's bytes, or nullptr if out of range or empty.
     */
    [[nodiscard]] uint8_t* mutable_frame_ptr(uint32_t frame_index);

    [[nodiscard]] uint32_t get_frame_capacity() const { return m_frame_capacity; }

    /**
     * @brief Ring slot the next capture is written to.
     */
    [[nodiscard]] uint32_t get_write_head() const { return m_write_head.load(); }

    /** @brief Advance the write head index, wrapping around frame_capacity. */
    void advance_write_head();

    /**
     * @brief Publish one capture as processed_data[0] and keep it in the history.
     *
     * Replaces the processed surface, invalidates the float views, and copies
     * the capture into the slot at the write head when its element type and
     * size match the container's surface, then advances the head. The capture
     * is always published as processed data; only the history copy is
     * conditional.
     *
     * @param frame Readback in the surface's native element type.
     * @return True if the capture was also kept in the history.
     */
    bool publish_capture(DataVariant frame);

    // =========================================================================
    // NDDimensionalContainer
    // =========================================================================

    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;

    /** @brief Elements in one surface: height * width * channels. */
    [[nodiscard]] uint64_t get_total_elements() const override;
    [[nodiscard]] MemoryLayout get_memory_layout() const override;
    void set_memory_layout(MemoryLayout layout) override;

    /**
     * @brief Extract data for all regions across all region groups that
     *        spatially intersect @p region.
     *        Crops from the last capture; no GPU work. Empty before the first
     *        capture.
     */
    [[nodiscard]] std::vector<DataVariant> get_region_data(const Region& region) const override;

    /**
     * @brief Upload the last capture to a container-owned VKImage.
     *
     * Requires at least one completed capture. Reuses the image while
     * dimensions and format match. The returned image is refreshed in place by
     * later calls, so it is a live image rather than a snapshot.
     *
     * @return Cached VKImage, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> to_image() const;

    /**
     * @brief Refresh the cached VKImage for ring slot @p frame_index.
     * @param frame_index Ring slot in [0, frame_capacity).
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> image_at(uint32_t frame_index) const;

    /**
     * @brief Crop a region from the last capture and refresh its cached VKImage.
     *
     * Region coordinates are [y, x] or [y, x, channel], inclusive. The
     * returned image dimensions are derived from the region extent. The image
     * is retained by the container and refreshed for any later region with the
     * same width and height.
     *
     * @param region Pixel rectangle. Must have at least 2 coordinates.
     * @return VKImage sized to the region, or nullptr on failure.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> region_to_image(const Region& region) const;

    void set_region_data(const Region& region, const std::vector<DataVariant>& data) override;

    [[nodiscard]] std::vector<DataVariant> get_region_group_data(const RegionGroup& group) const override;
    [[nodiscard]] std::vector<DataVariant> get_segments_data(const std::vector<RegionSegment>& segments) const override;

    /** @brief Element type of the surface, following the live format. */
    [[nodiscard]] std::type_index value_element_type() const override;

    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override;
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override;

    /**
     * @brief Drop the last capture. The history is kept.
     */
    void clear() override;

    /**
     * @brief Raw pointer to the last capture.
     *
     * Valid on the processing thread until the next capture or resize.
     */
    [[nodiscard]] const void* get_raw_data() const override;

    /** @brief True once a capture has been published since construction, clear() or a resize. */
    [[nodiscard]] bool has_data() const override;

    [[nodiscard]] ContainerDataStructure& get_structure() override { return m_structure; }
    [[nodiscard]] const ContainerDataStructure& get_structure() const override { return m_structure; }
    void set_structure(ContainerDataStructure s) override { m_structure = std::move(s); }

    // -------------------------------------------------------------------------
    // RegionGroup API - primary region registration interface.
    // -------------------------------------------------------------------------

    void add_region_group(const RegionGroup& group) override;
    [[nodiscard]] RegionGroup get_region_group(const std::string& name) const override;
    [[nodiscard]] std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;
    void remove_region_group(const std::string& name) override;

    /**
     * @brief No-op. All surface data is continuously available after capture.
     *        Register regions via add_region_group() instead.
     */
    void load_region(const Region& region) override;

    /**
     * @brief No-op. See load_region().
     */
    void unload_region(const Region& region) override;

    /**
     * @brief Always returns true. Surface data is available after any capture.
     */
    [[nodiscard]] bool is_region_loaded(const Region& region) const override;

    /**
     * @brief Reshape the surface and restart the history.
     *
     * Called by WindowAccessProcessor when the swapchain extent or format
     * changes, with the values the next capture is read at, so every history
     * slot matches the captures written into it. Drops the last capture.
     *
     * @param width  New surface width in pixels.
     * @param height New surface height in pixels.
     * @param format New swapchain surface format.
     */
    void handle_surface_resize(uint32_t width, uint32_t height, SurfaceFormat format);

    /**
     * @brief processed_data[frame_index] as a normalised float span.
     *
     * uint8_t values divided by 255.0f; uint16_t by 65535.0f; float is
     * zero-copy; packed 10-bit words are unpacked to four floats per pixel.
     * Returns an empty span if frame_index is out of range or the variant holds
     * a non-pixel type.
     *
     * Result is cached per slot and reused until the next capture,
     * invalidate_float_frame_cache() for that slot, or a resize. The span is
     * valid until then.
     *
     * @param frame_index Zero-based index into processed_data. Defaults to 0.
     * @return Normalised float span.
     */
    [[nodiscard]] std::span<const float> processed_frame_as_float(uint32_t frame_index = 0) const;

    /**
     * @brief Invalidate the normalised float cache for a specific processed_data slot.
     *
     * publish_capture() invalidates slot 0 itself. Call this after changing
     * processed_data by other means.
     *
     * @param frame_index Zero-based index into processed_data.
     */
    void invalidate_float_frame_cache(uint32_t frame_index = 0);

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

    /** @brief Elements in one surface. */
    [[nodiscard]] uint64_t get_frame_size() const override;

    /** @brief Captures held in the history, at most frame_capacity. */
    [[nodiscard]] uint64_t get_num_frames() const override;

    // -------------------------------------------------------------------------
    // Consumer tracking - dimension_index and reader_id are opaque slot handles.
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

protected:
    /** @brief The i-th oldest held capture. */
    [[nodiscard]] auto get_frame_span_impl(uint64_t frame_index) const -> DataSpanVariant override;
    void get_frames_impl(void* output, size_t count, uint64_t start_frame, uint64_t num_frames, const std::type_info& type) const override;

    /** @brief Coordinates are [y, x, channel] into processed_data[0]. */
    void get_value_impl(const std::vector<uint64_t>& coords,
        void* out, const std::type_info& type) const override;

    void set_value_impl(const std::vector<uint64_t>& coords,
        const void* in, const std::type_info& type) override { }

private:
    struct SurfaceLayout {
        uint32_t width { 0 };
        uint32_t height { 0 };
        SurfaceFormat format { SurfaceFormat::B8G8R8A8_SRGB };
        uint32_t channels { 4 };
        bool packed { false };

        [[nodiscard]] uint64_t elements() const
        {
            return static_cast<uint64_t>(width) * height * channels;
        }
    };

    struct PixelSnapshot {
        DataVariant data;
        std::vector<DataDimension> dimensions;
        SurfaceLayout layout;
    };

    std::shared_ptr<Core::Window> m_window;

    mutable Portal::Graphics::ImageCacheEntry m_surface_image;
    mutable std::vector<Portal::Graphics::ImageCacheEntry> m_frame_images;
    mutable std::map<std::pair<uint32_t, uint32_t>, Portal::Graphics::ImageCacheEntry> m_region_images;
    mutable Memory::Seqlock m_image_cache_lock;

    SurfaceLayout m_layout;
    ContainerDataStructure m_structure;
    std::vector<DataVariant> m_data;
    std::vector<DataVariant> m_processed_data;

    mutable Memory::Seqlock m_float_lock;
    mutable std::vector<std::vector<float>> m_normalised_cache;
    mutable std::vector<uint8_t> m_normalised_valid;

    std::unordered_map<std::string, RegionGroup> m_region_groups;

    std::shared_ptr<DataProcessor> m_default_processor;
    std::shared_ptr<DataProcessingChain> m_processing_chain;

    std::atomic<ProcessingState> m_processing_state { ProcessingState::IDLE };
    std::atomic<bool> m_ready_for_processing { false };

    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> m_state_callback;

    mutable Memory::Seqlock m_data_lock;
    mutable Memory::Seqlock m_region_lock;
    mutable Memory::Seqlock m_cb_lock;

    std::atomic<uint32_t> m_registered_readers { 0 };
    std::atomic<uint32_t> m_consumed_readers { 0 };
    std::atomic<uint32_t> m_next_reader_id { 0 };
    std::atomic<uint32_t> m_write_head { 0 };
    uint32_t m_frame_capacity { 1 };
    std::atomic<uint64_t> m_frames_written { 0 };

    void setup_dimensions(uint32_t width, uint32_t height, SurfaceFormat format);

    [[nodiscard]] uint64_t frames_held() const;

    [[nodiscard]] std::optional<PixelSnapshot> snapshot_pixels(
        std::optional<uint32_t> frame_index = std::nullopt) const;

    template <typename T>
    void copy_history(std::span<T> output, uint64_t start_frame, uint64_t num_frames) const;
};

} // namespace MayaFlux::Kakshya
