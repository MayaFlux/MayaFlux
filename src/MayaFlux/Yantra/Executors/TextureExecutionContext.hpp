#pragma once

#include "MayaFlux/Yantra/Executors/GpuExecutionContext.hpp"

#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"

namespace MayaFlux::Yantra {

/**
 * @class TextureExecutionContext
 * @brief GpuExecutionContext specialisation for image-only compute shaders.
 *
 * Operates on Datum<shared_ptr<SignalSourceContainer>> where the runtime
 * type is always TextureContainer.  Bypasses channel extraction entirely:
 * extract_inputs() returns empty channels and the input TextureContainer
 * is uploaded to a VKImage and staged as an IMAGE_STORAGE or IMAGE_SAMPLED
 * binding before dispatch_core runs.
 *
 * collect_gpu_outputs() ignores the float readback and instead wraps the
 * output VKImage (registered at binding index 0) in a new TextureContainer
 * via a blocking GPU->CPU download.
 *
 * calculate_dispatch_size() derives workgroup counts from the image
 * dimensions stored at construction, bypassing the structure-info path
 * which is meaningless for empty channels.
 *
 * Concrete texture ops subclass TextureOp (not this class directly).
 * They call stage_input() and set_push_constants() inside
 * on_before_gpu_dispatch(), then set_texture_backend() on the owning op.
 */
class MAYAFLUX_API TextureExecutionContext
    : public GpuExecutionContext<
          std::shared_ptr<Kakshya::SignalSourceContainer>,
          std::shared_ptr<Kakshya::SignalSourceContainer>> {
public:
    using ContainerDatum = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>;

    /**
     * @param config        Shader path, workgroup size, push constant size.
     * @param output_format Pixel format of the storage image created per dispatch.
     */
    explicit TextureExecutionContext(
        GpuShaderConfig config,
        Portal::Graphics::ImageFormat output_format)
        : GpuExecutionContext(std::move(config))
        , m_output_format(output_format)
    {
    }

    /**
     * @brief Stage the input TextureContainer as a VKImage for the given binding.
     *
     * Uploads pixel data to a new VKImage via TextureLoom and registers it as
     * IMAGE_SAMPLED at @p binding_index.  A default linear sampler from
     * SamplerForge is used.  Call from on_before_gpu_dispatch() in subclasses.
     *
     * @param container    Source TextureContainer.  Must be non-null.
     * @param binding_index Binding slot matching the IMAGE_SAMPLED declaration.
     */
    void stage_input(const Kakshya::TextureContainer& container, size_t binding_index, uint32_t layer = 0)
    {
        auto img = container.to_image(layer);
        auto sampler = Portal::Graphics::SamplerForge::instance().get_default_linear();
        stage_image_sampled(binding_index, std::move(img), sampler);
    }

protected:
    /**
     * @brief Returns empty channels -- no numeric extraction for image shaders.
     */
    std::pair<std::vector<std::vector<double>>, DataStructureInfo>
    extract_inputs(const ContainerDatum& /*input*/) override
    {
        return { {}, {} };
    }

    /**
     * @brief Derives dispatch groups from stored image dimensions.
     *
     * Ignores total_elements and structure_info (both meaningless for image
     * dispatch).  Uses m_width and m_height set by the most recent
     * prepare_output_image() call.
     */
    [[nodiscard]] std::array<uint32_t, 3> calculate_dispatch_size(
        size_t /*total_elements*/,
        const DataStructureInfo& /*structure_info*/) const override
    {
        const auto& ws = gpu_config().workgroup_size;
        return {
            (m_width + ws[0] - 1) / ws[0],
            (m_height + ws[1] - 1) / ws[1],
            1U
        };
    }

    /**
     * @brief Creates the output storage image and stages it at binding 0.
     *
     * Must be called by subclasses inside on_before_gpu_dispatch() after
     * dimensions are known, and before the base dispatch runs.
     *
     * @param width  Output image width in pixels.
     * @param height Output image height in pixels.
     */
    void prepare_output_image(uint32_t width, uint32_t height)
    {
        m_width = width;
        m_height = height;
        auto out = Portal::Graphics::TextureLoom::instance()
                       .create_storage_image(width, height, m_output_format);
        stage_image_storage(0, std::move(out));
    }

    /**
     * @brief Wraps the written output image in a TextureContainer.
     *
     * Ignores the float readback in @p raw.  Downloads the storage image at
     * binding 0 into a new TextureContainer and returns it as a ContainerDatum.
     */
    ContainerDatum collect_gpu_outputs(
        const GpuChannelResult& /*raw*/,
        const std::vector<std::vector<double>>& /*channels*/,
        const DataStructureInfo& /*structure_info*/) override
    {
        auto img = get_output_image(0);
        if (!img) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "TextureExecutionContext: no output image at binding 0 after dispatch");
        }

        Portal::Graphics::TextureLoom::instance().transition_layout(
            img, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);

        auto container = std::make_shared<Kakshya::TextureContainer>(m_width, m_height, m_output_format);

        container->from_image(img, 0);
        return ContainerDatum { std::static_pointer_cast<Kakshya::SignalSourceContainer>(container) };
    }

private:
    Portal::Graphics::ImageFormat m_output_format;
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
};

} // namespace MayaFlux::Yantra
