#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Buffers {

class TextureBuffer;

/**
 * @class DataWriteProcessor
 * @brief Modality-aware multi-slot write processor for plain VKBuffer.
 *
 * Accepts one or more DataVariant values per cycle. Slot 0 is the primary
 * upload path and is routed by the attached buffer's DataModality:
 *
 *   - Vertex modalities (VERTEX_POSITIONS_3D, VERTEX_NORMALS_3D,
 *     VERTEX_TANGENTS_3D, VERTEX_COLORS_RGB, VERTEX_COLORS_RGBA,
 *     TEXTURE_COORDS_2D): routed through Kakshya::as_vertex_access().
 *     VertexLayout derived from the result is written back to the buffer
 *     via set_vertex_layout() so RenderProcessor has correct stride and
 *     attribute information.
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
     * @brief Configure the pixel upload path for texture modalities.
     *
     * Must be called before the first set_data() when the attached buffer
     * has a texture modality and is not a TextureBuffer. For TextureBuffer
     * targets this call is unnecessary: set_data() delegates directly to
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

    std::weak_ptr<TextureBuffer> m_texture_buffer;

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
