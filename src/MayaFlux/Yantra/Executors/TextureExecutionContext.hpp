#pragma once

#include "GpuExecutionContext.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Yantra {

/**
 * @class TextureExecutionContext
 * @brief GpuExecutionContext specialisation for image compute shaders.
 *
 * Accepts DataIO (Datum<vector<DataVariant>>) as both input and output,
 * matching the standard ComputeOperation contract.  Image staging is driven
 * by the optional container field on the input Datum:
 *
 *   - When datum.container holds a TextureContainer, extract_inputs() stashes
 *     it and on_before_gpu_dispatch() uploads it to a VKImage staged at the
 *     declared image input binding.  This is the preferred path: the container
 *     carries format, layer count, mip metadata, and sampler config.
 *
 *   - When datum.container is absent or not a TextureContainer, no image
 *     staging occurs.  The shader receives whatever bindings the caller
 *     declared via set_binding_data / stage_passthrough / stage_image_sampled
 *     directly.  dispatch_async still works; it is a no-op on the image side.
 *
 * Output options (select one at construction):
 *
 *   OutputMode::CONTAINER — the storage image at binding 0 is downloaded into
 *   a new TextureContainer and placed in the output Datum's container field.
 *   Primary float readback is empty.  Mirrors the original behaviour for
 *   texture-transform pipelines (blur, colour grade, format convert).
 *
 *   OutputMode::SCALAR — no image download.  collect_result() returns the
 *   primary float readback and aux SSBOs from GpuChannelResult, exactly as
 *   ShaderExecutionContext does.  Use for reduction shaders that write a small
 *   SSBO (spatial average, histogram, region sample) and feed the node graph.
 *
 * dispatch_async / collect_result mirror ShaderExecutionContext so
 * GpuComputeNode can own this class without modification.
 *
 * calculate_dispatch_size() uses image dimensions when a TextureContainer is
 * present, otherwise falls through to the standard element-count path.
 */
class MAYAFLUX_API TextureExecutionContext
    : public GpuExecutionContext<
          std::vector<Kakshya::DataVariant>,
          std::vector<Kakshya::DataVariant>> {
public:
    using Base = GpuExecutionContext<
        std::vector<Kakshya::DataVariant>,
        std::vector<Kakshya::DataVariant>>;

    enum class OutputMode : uint8_t {
        CONTAINER, ///< Download storage image at binding 0 into a TextureContainer.
        SCALAR, ///< Return SSBO readback via collect_result(); no image download.
        IMAGE, ///< Transition output image to shader-read layout; no CPU download.
               ///< Retrieve via get_output_image(0). Zero readback cost.
    };

    /**
     * @param config        Shader path, workgroup size, push constant size.
     * @param output_format Pixel format of the storage image created per dispatch.
     *                      Ignored when mode is SCALAR.
     * @param mode          Controls what collect_gpu_outputs / collect_result return.
     * @param image_binding Binding index at which the input image is staged.
     *                      Default 1.
     * @param aux_bindings  Additional buffer bindings to declare.
     * @param image_access  Access mode for the input image binding: IMAGE_SAMPLED
     *                      (default) or IMAGE_STORAGE.
     * @param output_binding Binding index for the output storage image.
     *                      Ignored when mode is SCALAR. Default 0.
     */
    explicit TextureExecutionContext(
        GpuShaderConfig config,
        Portal::Graphics::ImageFormat output_format = Portal::Graphics::ImageFormat::RGBA8,
        OutputMode mode = OutputMode::CONTAINER,
        uint32_t image_binding = 1,
        std::vector<GpuBufferBinding> aux_bindings = {},
        GpuBufferBinding::ElementType image_access = GpuBufferBinding::ElementType::IMAGE_SAMPLED,
        uint32_t output_binding = 0)
        : Base(std::move(config))
        , m_output_format(output_format)
        , m_output_mode(mode)
        , m_aux_bindings(std::move(aux_bindings))
    {
        m_image_slots.push_back({ .binding = { .set = 0,
                                      .binding = image_binding,
                                      .direction = GpuBufferBinding::Direction::INPUT,
                                      .element_type = image_access } });
        if (m_output_mode != OutputMode::SCALAR) {
            m_image_slots.push_back({ .binding = { .set = 0,
                                          .binding = output_binding,
                                          .direction = GpuBufferBinding::Direction::OUTPUT,
                                          .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE } });
        }
    }

    /**
     * @brief Set the array layer staged from the input TextureContainer on the
     *        next dispatch_async call.
     *
     * Defaults to 0.  Call before dispatch_async when iterating video frames,
     * animation layers, or any multi-layer container where the layer index
     * changes per dispatch.  Has no effect when no TextureContainer is present
     * on the input Datum.
     *
     * @param layer Array layer index. Must be < TextureContainer::get_layer_count().
     */
    void set_input_layer(uint32_t layer) { m_pending_layer = layer; }

    /**
     * @brief Override the output storage image dimensions for the next dispatch.
     *
     * When set, on_before_gpu_dispatch allocates the output image at these
     * dimensions rather than the input container dimensions, and
     * calculate_dispatch_size dispatches workgroups to cover this extent.
     * Callers are responsible for supplying matching push constants so the
     * shader maps output pixels to the correct source coordinates.
     *
     * Pass std::nullopt to restore container-derived sizing.
     *
     * @param w Output width in pixels.
     * @param h Output height in pixels.
     */
    void set_output_dimensions(uint32_t w, uint32_t h)
    {
        m_output_dim_override = { w, h };
    }

    void clear_output_dimensions()
    {
        m_output_dim_override = std::nullopt;
    }

    // =========================================================================
    // Async dispatch — mirrors ShaderExecutionContext
    // =========================================================================

    /**
     * @brief Non-blocking dispatch.
     *
     * If datum.container holds a TextureContainer it is uploaded and staged
     * before submission.  Fence becomes signaled when GPU work completes.
     * Call collect_result() (SCALAR mode) or retrieve the output Datum via
     * execute() (CONTAINER mode) once signaled.
     *
     * @param input DataIO whose container field drives optional image staging.
     * @return FenceID to poll with ShaderFoundry::is_fence_signaled.
     */
    [[nodiscard]] Portal::Graphics::FenceID dispatch_async(const input_type& input)
    {
        if (!ensure_gpu_ready())
            return Portal::Graphics::INVALID_FENCE;

        auto [channels, structure_info] = extract_inputs(input);
        return dispatch_core_async(channels, structure_info);
    }

    /**
     * @brief Collect SSBO readback after a signaled async dispatch.
     *
     * Valid only in SCALAR mode.  Mirrors ShaderExecutionContext::collect_result().
     *
     * @return GpuChannelResult with primary float data and aux buffers.
     */
    [[nodiscard]] GpuChannelResult collect_result()
    {
        GpuChannelResult result;
        result.primary = readback_primary(last_effective_element_count());
        readback_aux(result);
        return result;
    }

    /**
     * @brief Collect the output TextureContainer after a signaled async dispatch.
     *
     * Valid only in CONTAINER mode. Mirrors collect_result() for SCALAR mode.
     * Must be called only after ShaderFoundry::is_fence_signaled returns true
     * for the FenceID returned by dispatch_async.
     *
     * @return DataIO with container field holding the output TextureContainer,
     *         or empty DataIO on failure.
     */
    [[nodiscard]] output_type collect_container_result()
    {
        GpuChannelResult raw;
        readback_aux(raw);
        return collect_gpu_outputs(raw, {}, {});
    }

    // =========================================================================
    // Manual image staging — for callers that already hold a VKImage
    // =========================================================================

    /**
     * @brief Stage an arbitrary VKImage at the declared input image binding.
     *
     * Bypasses container resolution. The image must be in
     * eShaderReadOnlyOptimal layout for IMAGE_SAMPLED access, or eGeneral
     * for IMAGE_STORAGE access. Use before dispatch_async when the source
     * is a render-pass output or camera frame rather than a container.
     *
     * @param image   Initialized VKImage.
     * @param sampler Vulkan sampler. Defaults to the TextureLoom linear sampler
     *                when nullptr. Ignored for IMAGE_STORAGE access.
     */
    void stage_image(
        const std::shared_ptr<Core::VKImage>& image,
        vk::Sampler sampler = nullptr)
    {
        auto& slot = input_slot();
        if (slot.binding.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE) {
            stage_image_at(slot.binding.binding, image, GpuBufferBinding::ElementType::IMAGE_STORAGE);
        } else {
            auto s = sampler
                ? sampler
                : Portal::Graphics::SamplerForge::instance().get_default_linear();
            stage_image_at(slot.binding.binding, image, GpuBufferBinding::ElementType::IMAGE_SAMPLED, s);
        }
        slot.image = image;
        m_pending_container = nullptr;
    }

    /**
     * @brief Stage a TextureContainer layer at the declared input image binding.
     *
     * Convenience for callers that hold a container and want explicit control
     * over layer and sampler rather than relying on Datum.container resolution.
     * Always stages as IMAGE_SAMPLED regardless of the input slot's declared
     * access mode.
     *
     * @param container Source TextureContainer.
     * @param layer     Array layer index (default 0).
     * @param sampler   Vulkan sampler. Defaults to linear when nullptr.
     */
    void stage_container(
        const Kakshya::TextureContainer& container,
        uint32_t layer = 0,
        vk::Sampler sampler = nullptr)
    {
        auto& slot = input_slot();
        auto img = container.to_image(layer);
        auto s = sampler
            ? sampler
            : Portal::Graphics::SamplerForge::instance().get_default_linear();
        stage_image_at(slot.binding.binding, img, GpuBufferBinding::ElementType::IMAGE_SAMPLED, s);
        slot.image = img;
        m_pending_container = nullptr;
    }

    /**
     * @brief Allocates the output storage image and stages it at the
     *        declared output binding.
     *
     * Only called in CONTAINER or IMAGE mode, from on_before_gpu_dispatch
     * or per-step by callers chaining multi-pass sequences. Caches by
     * dimension: reallocates only when width or height change from the
     * previously staged output.
     */
    void prepare_output_image(uint32_t width, uint32_t height)
    {
        auto& slot = output_slot();
        if (!slot.image || slot.width != width || slot.height != height) {
            slot.image = Portal::Graphics::TextureLoom::instance()
                             .create_storage_image(width, height, m_output_format);
            slot.width = width;
            slot.height = height;
        }
        stage_image_at(slot.binding.binding, slot.image, GpuBufferBinding::ElementType::IMAGE_STORAGE);
    }

protected:
    // =========================================================================
    // GpuExecutionContext overrides
    // =========================================================================

    /**
     * @brief Declare the image bindings from m_image_slots, plus any
     *        aux SSBO bindings provided at construction.
     */
    [[nodiscard]] std::vector<GpuBufferBinding> declare_buffer_bindings() const override
    {
        std::vector<GpuBufferBinding> bindings;
        for (const auto& slot : m_image_slots)
            bindings.push_back(slot.binding);
        bindings.insert(bindings.end(), m_aux_bindings.begin(), m_aux_bindings.end());
        return bindings;
    }

    /**
     * @brief Stashes the TextureContainer from datum.container for use in
     *        on_before_gpu_dispatch; returns empty channels (image shaders
     *        do not use the numeric channel path).
     */
    std::pair<std::vector<std::vector<double>>, DataStructureInfo>
    extract_inputs(const input_type& input) override
    {
        m_pending_container = resolve_texture_container(input);
        return { {}, {} };
    }

    /**
     * @brief Stages the pending TextureContainer at the declared input image
     *        binding and, in CONTAINER or IMAGE mode, allocates the output
     *        storage image at the declared output binding.
     */
    void on_before_gpu_dispatch(
        const std::vector<std::vector<double>>& /*channels*/,
        const DataStructureInfo& /*structure_info*/) override
    {
        if (m_pending_container) {
            const auto sampler = Portal::Graphics::SamplerForge::instance().get_default_linear();
            std::shared_ptr<Core::VKImage> img;
            if (m_pending_container->get_layer_count() > 1) {
                if (!m_upload_staging) {
                    m_upload_staging = Buffers::create_image_staging_buffer(
                        m_pending_container->byte_size() * m_pending_container->get_layer_count());
                }
                img = m_pending_container->to_image_array(m_upload_staging);
            } else {
                if (!m_upload_staging) {
                    m_upload_staging = Buffers::create_image_staging_buffer(
                        m_pending_container->byte_size());
                }
                img = m_pending_container->to_image(m_pending_layer, m_upload_staging);
            }

            auto& in_slot = input_slot();
            if (in_slot.binding.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE) {
                stage_image_at(in_slot.binding.binding, img, GpuBufferBinding::ElementType::IMAGE_STORAGE);
            } else {
                stage_image_at(in_slot.binding.binding, img, GpuBufferBinding::ElementType::IMAGE_SAMPLED, sampler);
            }
            in_slot.image = img;

            if (m_output_mode == OutputMode::CONTAINER
                || m_output_mode == OutputMode::IMAGE) {
                const uint32_t ow = m_output_dim_override
                    ? m_output_dim_override->first
                    : m_pending_container->get_width();
                const uint32_t oh = m_output_dim_override
                    ? m_output_dim_override->second
                    : m_pending_container->get_height();
                prepare_output_image(ow, oh);
            }
        }
    }

    /**
     * @brief Derives dispatch size from the staged container dimensions when
     *        available; falls back to element-count dispatch otherwise.
     */
    [[nodiscard]] std::array<uint32_t, 3> calculate_dispatch_size(
        size_t total_elements,
        const DataStructureInfo& structure_info) const override
    {
        const auto& ws = gpu_config().workgroup_size;
        if (m_output_dim_override) {
            const uint32_t w = m_output_dim_override->first;
            const uint32_t h = m_output_dim_override->second;
            return {
                (w + ws[0] - 1) / ws[0],
                (h + ws[1] - 1) / ws[1],
                1U
            };
        }
        if (m_pending_container) {
            const uint32_t w = m_pending_container->get_width();
            const uint32_t h = m_pending_container->get_height();
            return {
                (w + ws[0] - 1) / ws[0],
                (h + ws[1] - 1) / ws[1],
                1U
            };
        }
        return Base::calculate_dispatch_size(total_elements, structure_info);
    }

    /**
     * @brief In CONTAINER mode: downloads the storage image at the declared
     *        output binding into a new TextureContainer placed in output.container.
     *        In SCALAR mode: returns an empty DataIO (use collect_result() instead).
     */
    output_type collect_gpu_outputs(
        const GpuChannelResult& /*raw*/,
        const std::vector<std::vector<double>>& /*channels*/,
        const DataStructureInfo& /*structure_info*/) override
    {
        if (m_output_mode == OutputMode::SCALAR)
            return output_type {};

        auto img = get_output_image(output_slot().binding.binding);
        if (!img) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "TextureExecutionContext: no output image at declared output binding after dispatch");
        }

        Portal::Graphics::TextureLoom::instance().transition_layout(
            img, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);

        if (m_output_mode == OutputMode::IMAGE)
            return output_type {};

        const uint32_t w = m_output_dim_override
            ? m_output_dim_override->first
            : (m_pending_container ? m_pending_container->get_width() : img->get_width());
        const uint32_t h = m_output_dim_override
            ? m_output_dim_override->second
            : (m_pending_container ? m_pending_container->get_height() : img->get_height());

        if (!m_output_container) {
            m_output_container = std::make_shared<Kakshya::TextureContainer>(
                w, h, m_output_format);
        }

        if (!m_download_staging) {
            m_download_staging = Buffers::create_image_staging_buffer(
                m_output_container->byte_size());
        }

        m_output_container->from_image(img, m_download_staging, 0);

        output_type result;
        result.container = std::static_pointer_cast<Kakshya::SignalSourceContainer>(
            m_output_container);
        return result;
    }

private:
    /**
     * @struct ImageSlot
     * @brief One declared image binding: its GpuBufferBinding descriptor,
     *        the currently staged VKImage, and cached dimensions.
     *
     * direction on the stored GpuBufferBinding distinguishes the input slot
     * from the output slot; element_type distinguishes IMAGE_STORAGE from
     * IMAGE_SAMPLED. Binding index is caller-configurable at construction,
     * never hardcoded elsewhere in this class.
     */
    struct ImageSlot {
        GpuBufferBinding binding;
        std::shared_ptr<Core::VKImage> image;
        uint32_t width {};
        uint32_t height {};
    };

    Portal::Graphics::ImageFormat m_output_format;
    OutputMode m_output_mode;
    uint32_t m_pending_layer {};
    std::vector<ImageSlot> m_image_slots;

    std::shared_ptr<Kakshya::TextureContainer> m_pending_container;
    std::shared_ptr<Kakshya::TextureContainer> m_output_container;
    std::shared_ptr<Buffers::VKBuffer> m_download_staging;
    std::shared_ptr<Buffers::VKBuffer> m_upload_staging;
    std::vector<GpuBufferBinding> m_aux_bindings;

    std::optional<std::pair<uint32_t, uint32_t>> m_output_dim_override;

    // =========================================================================
    // Helpers
    // =========================================================================

    /**
     * @brief Find the declared input image slot.
     * @return Reference to the ImageSlot with Direction::INPUT.
     */
    [[nodiscard]] ImageSlot& input_slot()
    {
        for (auto& s : m_image_slots) {
            if (s.binding.direction == GpuBufferBinding::Direction::INPUT)
                return s;
        }

        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "TextureExecutionContext: no input image slot declared");
    }

    /**
     * @brief Find the declared output image slot.
     * @return Reference to the ImageSlot with Direction::OUTPUT.
     * @note Only present when constructed with mode != SCALAR.
     */
    [[nodiscard]] ImageSlot& output_slot()
    {
        for (auto& s : m_image_slots) {
            if (s.binding.direction == GpuBufferBinding::Direction::OUTPUT)
                return s;
        }

        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "TextureExecutionContext: no output image slot declared (SCALAR mode?)");
    }

    /**
     * @brief Extracts a TextureContainer pointer from datum.container if present.
     *
     * Returns nullptr when the container field is absent or holds a different
     * SignalSourceContainer subtype.
     */
    static std::shared_ptr<Kakshya::TextureContainer> resolve_texture_container(const input_type& input)
    {
        if (!input.container || !*input.container)
            return nullptr;
        return std::dynamic_pointer_cast<Kakshya::TextureContainer>(input.container.value());
    }
};

} // namespace MayaFlux::Yantra
