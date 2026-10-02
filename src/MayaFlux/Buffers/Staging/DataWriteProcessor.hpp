#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/ImageData.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class DataWriteProcessor
 * @brief Modality-aware multi-slot write processor for plain VKBuffer.
 *
 * Accepts one or more DataVariant values per cycle. Slot 0 is the primary
 * upload path and is routed by the attached buffer's DataModality:
 *
 *   - VERTICES_3D and single-attribute vertex modalities (VERTEX_POSITIONS_3D,
 *     VERTEX_NORMALS_3D, VERTEX_TANGENTS_3D, VERTEX_COLORS_RGB,
 *     VERTEX_COLORS_RGBA, TEXTURE_COORDS_2D): all slots are passed as an
 *     ordered channel span to as_*_vertex_access(). Full 60-byte vertices
 *     are always assembled; partial channel sets are completed from
 *     VertexAccessConfig defaults.
 *
 *   - Texture modalities (IMAGE_2D, IMAGE_COLOR, TEXTURE_2D):
 *     routed through Kakshya::as_texture_access(). GpuDataFormat from the
 *     result is available via last_texture_format() for pipeline configuration.
 *
 *   - All other modalities: raw bytes via DataAccess::gpu_buffer().
 *     Zero-copy when the variant type matches the target layout.
 *
 * Slot 1..N are uploaded raw via DataAccess::gpu_buffer() regardless of
 * modality. These are intended for secondary data (e.g. a texture payload
 * alongside vertex data) where the caller has configured the corresponding
 * descriptor bindings on the RenderProcessor.
 *
 * Modality is read from the attached VKBuffer in on_attach(). The processor
 * warns if the supplied DataVariant is incompatible with that modality.
 *
 * Dirty gating: upload occurs only when set_data() has been called since
 * the last cycle. No stale re-upload.
 *
 * Thread safety: set_data() and the graphics thread may run concurrently.
 * Lock-free double-buffer swap via atomic_flag ensures the graphics thread
 * never blocks on the supplier thread.
 *
 * Staging: a persistent host-visible staging buffer is allocated on
 * on_attach() when the target is device-local, and reused every cycle.
 *
 * The write_* requests reach targets beyond the primary storage: another
 * VKBuffer, a named field, a raw back_buffers slot, mesh geometry, and
 * texture pixels. Each is a one-shot request applied at the start of the next
 * graphics cycle, ahead of the set_* upload, in the order it was made. Each
 * writes to the authority of its target, never to a GPU copy that its owner
 * regenerates: meshes go to the owning MeshData or node, retained pixels to the
 * TextureBuffer, and the rest to the GPU storage their owners read from. Attach
 * the processor before the producers that consume the written data. Requests
 * come from one supplier thread, as with set_data().
 *
 * Intended for use with plain VKBuffer where no specialised child class
 * exists for the data being contributed. The caller is responsible for
 * configuring a matching RenderProcessor via VKBuffer::setup_rendering().
 */
class MAYAFLUX_API DataWriteProcessor : public VKBufferProcessor {
public:
    DataWriteProcessor();
    ~DataWriteProcessor() override = default;

    /**
     * @brief Supply a single data value for the next cycle (slot 0).
     *
     * Thread-safe. The variant is moved into the pending slot and the dirty
     * flag is set. Conversion to the upload representation is deferred to
     * processing_function() on the graphics thread.
     *
     * @param variant DataVariant compatible with the attached buffer's modality.
     */
    void set_data(Kakshya::DataVariant variant);

    /**
     * @brief Supply multiple data values for the next cycle.
     *
     * Slot 0 is the primary upload path (modality-routed). Slots 1..N are
     * uploaded raw and correspond to secondary resources (e.g. texture data
     * alongside vertex data). The caller is responsible for ensuring the
     * attached buffer and its RenderProcessor have been configured with
     * matching descriptor bindings for any secondary slots.
     *
     * Thread-safe. The full vector is swapped atomically on the next
     * processing_function() call.
     *
     * @param variants One or more DataVariant values. Must not be empty.
     */
    void set_data(std::vector<Kakshya::DataVariant> variants);

    /**
     * @brief Supply pre-packed interleaved vertex bytes for the next cycle.
     *
     * data must point to N contiguous 60-byte Vertex records.
     * byte_count must be a multiple of 60. Routed through the
     * VERTICES_3D path: no channel assembly, no conversion.
     *
     * Thread-safe. The bytes are copied into the pending slot and the
     * dirty flag is set. Upload is deferred to processing_function()
     * on the graphics thread.
     *
     * @param data       Pointer to interleaved vertex data.
     * @param byte_count Total size in bytes; must be a multiple of 60.
     */
    void set_vertices(const void* data, size_t byte_count);

    /**
     * @brief Supply typed vertex data for the next cycle.
     *
     * Reinterprets the span as raw bytes and delegates to set_vertices(void*, size_t).
     * T must be exactly 60 bytes: one of Kakshya::Vertex, PointVertex, LineVertex,
     * MeshVertex, or any user struct with the canonical layout.
     *
     * @tparam T 60-byte vertex type.
     * @param vertices Source span.
     */
    template <typename T>
    void set_vertices(std::span<const T> vertices)
    {
        static_assert(sizeof(T) == 60, "set_vertices: T must be a 60-byte vertex type");
        set_vertices(vertices.data(), vertices.size_bytes());
    }

    /**
     * @brief Returns true if a snapshot has been set and not yet consumed.
     */
    [[nodiscard]] bool has_pending() const noexcept;

    /**
     * @brief Supply a texture to bind on the next graphics tick.
     *
     * Stores the image and binding name behind an atomic dirty flag.
     * On the next processing_function() call the image is bound to the
     * buffer's RenderProcessor via bind_texture(). The descriptor slot
     * must already exist in the pipeline (declared in the RenderConfig
     * ShaderConfig bindings). Calling again replaces the pending binding
     * before it is consumed.
     *
     * @param image   GPU image. nullptr clears the binding.
     * @param binding Descriptor name matching the fragment shader.
     */
    void set_texture(std::shared_ptr<Core::VKImage> image, std::string binding);

    /**
     * @brief Supply pixel data for the co-resident texture on the next cycle.
     *
     * Independent of the vertex data path. Requires setup_pixel_target() to
     * have been called. On the next processing_function() call the variant
     * is routed through as_texture_access() and uploaded to the GPU texture
     * bound at the configured binding.
     *
     * Thread-safe. The variant is swapped atomically behind m_pixel_dirty.
     *
     * @param variant Pixel data compatible with the format passed to setup_pixel_target().
     */
    void set_pixel_data(Kakshya::DataVariant variant);

    /**
     * @brief Configure the pixel upload path for texture modalities.
     *
     * Must be called before the first set_data() when the attached buffer
     * has a texture modality and is not a TextureBuffer. For TextureBuffer
     * targets, call this directly
     * TextureBuffer::set_pixel_data().
     *
     * @param width  Texture width in texels.
     * @param height Texture height in texels.
     * @param format Pixel format.
     * @param binding Descriptor name to bind the VKImage to on the RenderProcessor.
     */
    void setup_pixel_target(
        uint32_t width,
        uint32_t height,
        Portal::Graphics::ImageFormat format,
        std::string binding = "texSampler");

    /**
     * @brief Returns the GpuDataFormat resolved on the last texture upload.
     *
     * Valid only when the attached buffer has a texture modality and at least
     * one successful upload has occurred. Returns GpuDataFormat::VEC4_F32
     * as a default before first upload.
     */
    [[nodiscard]] Kakshya::GpuDataFormat last_texture_format() const noexcept;

    /**
     * @brief Write bytes into another VKBuffer, such as a plain descriptor-bound buffer.
     *
     * The target is not resized. A device-local target is written through a
     * persistent staging buffer. A buffer whose descriptor binding has a node,
     * audio, or host-buffer source is rewritten from that source each cycle, so
     * write to the source instead.
     *
     * @param target     Buffer to write; retained until the request runs.
     * @param data       Source data, converted to bytes when the request runs.
     * @param dst_offset Byte offset into @p target. offset plus data size must fit its allocation.
     */
    void write_buffer(std::shared_ptr<VKBuffer> target, Kakshya::DataVariant data, size_t dst_offset = 0);

    /**
     * @brief Write a named field of the attached buffer.
     *
     * A VolumeGridBuffer field is written through seed_raw, and a
     * NetworkGeometryBuffer state field into the slot its next stage reads.
     * The data size must equal the field's byte size.
     */
    void write_field(std::string name, Kakshya::DataVariant data);

    /**
     * @brief Write a raw back_buffers slot of the attached buffer.
     * @param index Slot index into its back_buffers.
     * @param data  Source data; a slot does not carry its size, so the data must
     *              not exceed what the buffer's owner allocated for it.
     */
    void write_back_buffer(size_t index, Kakshya::DataVariant data);

    /**
     * @brief Write mesh geometry to the attached buffer's authoritative copy.
     *
     * A MeshBuffer takes it as its MeshData, and a GeometryBuffer driven by a
     * MeshWriterNode sets it on that node. A MeshNetworkBuffer needs @p slot to
     * name the slot whose node is set. No transform is applied.
     */
    void write_mesh(Kakshya::MeshData data, std::string slot = {});

    /**
     * @brief Write the retained host pixels of the attached TextureBuffer.
     *
     * The image must match the texture's format and size. The buffer re-uploads
     * to its GPU image on its own cycle.
     */
    void write_pixels(Kakshya::ImageData image);

    /**
     * @brief Write pixels straight into the attached TextureBuffer's GPU image.
     *
     * The image must match the GPU image's format and size, which must be
     * writable (transfer-destination usage). Retained host pixels that are
     * marked dirty re-upload over this write.
     */
    void write_texture_pixels(Kakshya::ImageData image);

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    struct PendingTexture {
        std::shared_ptr<Core::VKImage> image;
        std::string binding;
    };

    std::optional<PendingTexture> m_pending_texture;
    std::atomic_flag m_texture_dirty;

    Kakshya::DataVariant m_pixel_pending;
    Kakshya::DataVariant m_pixel_active;
    std::atomic_flag m_pixel_dirty;
    bool m_tex_binding_confirmed {};

    uint32_t m_tex_width {};
    uint32_t m_tex_height {};
    Portal::Graphics::ImageFormat m_tex_format { Portal::Graphics::ImageFormat::RGBA32F };
    std::string m_tex_binding;
    std::shared_ptr<Core::VKImage> m_gpu_texture;
    std::shared_ptr<VKBuffer> m_image_staging;

    Kakshya::DataModality m_modality { Kakshya::DataModality::UNKNOWN };
    Kakshya::GpuDataFormat m_last_texture_format { Kakshya::GpuDataFormat::VEC4_F32 };
    Portal::Graphics::PrimitiveTopology m_topology { Portal::Graphics::PrimitiveTopology::POINT_LIST };

    std::vector<Kakshya::DataVariant> m_data_pending;
    std::vector<Kakshya::DataVariant> m_active;
    std::atomic_flag m_data_dirty;

    std::shared_ptr<VKBuffer> m_staging;

    using WriteFn = std::function<void(DataWriteProcessor&, const std::shared_ptr<VKBuffer>&)>;
    static constexpr size_t k_write_queue_capacity = 64;

    std::unique_ptr<Memory::LockFreeQueue<WriteFn, k_write_queue_capacity>> m_writes;
    std::shared_ptr<VKBuffer> m_write_staging;
    std::shared_ptr<VKBuffer> m_write_image_staging;

    void enqueue_write(WriteFn fn);

    void upload_primary(const std::shared_ptr<VKBuffer>& vk, std::vector<Kakshya::DataVariant>& slots);
    void upload_secondary(const std::shared_ptr<VKBuffer>& vk, Kakshya::DataVariant& slot);
    void upload_vertex(const std::shared_ptr<VKBuffer>& vk, std::vector<Kakshya::DataVariant>& slots);
    void upload_texture(const std::shared_ptr<VKBuffer>& vk, Kakshya::DataVariant& slot);
    void upload_raw(const std::shared_ptr<VKBuffer>& vk, Kakshya::DataVariant& slot);

    void ensure_capacity(const std::shared_ptr<VKBuffer>& vk, size_t required);

    static bool is_vertex_modality(Kakshya::DataModality m) noexcept;
    static bool is_texture_modality(Kakshya::DataModality m) noexcept;
};

} // namespace MayaFlux::Buffers
