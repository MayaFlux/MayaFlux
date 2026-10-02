#include "TextureArrayBuffer.hpp"

#include "TextureProcessor.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

TextureArrayBuffer::TextureArrayBuffer(
    uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    uint32_t layers)
    : TextureBuffer(width, height, format)
    , m_layers(std::max(layers, 1U))
    , m_layer_bytes(static_cast<size_t>(width) * height * Portal::Graphics::TextureLoom::get_bytes_per_pixel(format))
{
    RenderConfig config;
    config.vertex_shader = "texture.vert.spv";
    config.fragment_shader = "texture_array.frag.spv";
    config.default_texture_binding = "textureArray";
    config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP;
    set_default_render_config(config);

    m_push.layer_count = m_layers;
    m_push.weights.fill(1.0F);
}

TextureArrayBuffer::TextureArrayBuffer(
    const Kakshya::VideoStreamSpec& spec,
    std::optional<Portal::Graphics::FitMode> fit)
    : TextureArrayBuffer(spec.width, spec.height, spec.format, static_cast<uint32_t>(spec.ring_frames))
{
    m_fit = fit;
}

void TextureArrayBuffer::setup_processors(ProcessingToken token)
{
    ensure_array();
    TextureBuffer::setup_processors(token);

    if (const auto processor = get_texture_processor()) {
        processor->set_streaming_mode(true);
    }
}

void TextureArrayBuffer::setup_rendering(const RenderConfig& config)
{
    ensure_array();
    TextureBuffer::setup_rendering(config);

    if (const auto processor = get_render_processor()) {
        processor->set_push_constant_size(m_custom_push.empty() ? sizeof(Push) : m_custom_push.size());
    }
    flush_push();
    bind_layer_data();
}

bool TextureArrayBuffer::submit_layer(uint32_t layer, std::span<const uint8_t> pixels)
{
    if (layer >= m_layers || pixels.size() != m_layer_bytes) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureArrayBuffer::submit_layer needs a layer below {} and {} bytes, got layer {} and {} bytes",
            m_layers, m_layer_bytes, layer, pixels.size());
        return false;
    }

    if (m_layer_mode) {
        if (!m_layer_staging) {
            m_layer_staging = create_image_staging_buffer(m_layer_bytes);
        }
        return Portal::Graphics::get_texture_manager().upload_layer(
            get_texture(), layer, pixels.data(), pixels.size(), m_layer_staging);
    }

    auto& storage = get_pixel_data_mutable();
    if (storage.size() != m_layer_bytes * m_layers) {
        storage.assign(m_layer_bytes * m_layers, 0);
    }

    std::memcpy(storage.data() + static_cast<size_t>(layer) * m_layer_bytes, pixels.data(), m_layer_bytes);
    mark_pixels_dirty();
    return true;
}

bool TextureArrayBuffer::submit_layer(uint32_t layer, const Kakshya::ImageData& image)
{
    if (layer >= m_layers || image.byte_size() == 0 || !image.is_consistent()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureArrayBuffer::submit_layer needs a layer below {} and a consistent image, got layer {}",
            m_layers, layer);
        return false;
    }

    const bool same_extent = image.width == get_width() && image.height == get_height();
    if (same_extent && image.format == get_format()) {
        return submit_layer(layer,
            std::span<const uint8_t>(static_cast<const uint8_t*>(image.data()), image.byte_size()));
    }

    if (!same_extent && !fit_for(layer)) {
        return false;
    }

    if (!m_source_staging || m_source_staging->get_size() < image.byte_size()) {
        m_source_staging = create_image_staging_buffer(image.byte_size());
    }

    const Portal::Graphics::ImageKey key {
        .width = image.width,
        .height = image.height,
        .format = image.format
    };

    const auto source = Portal::Graphics::get_texture_manager().refresh_cached_image(
        m_source_cache, key,
        std::span<const uint8_t>(static_cast<const uint8_t*>(image.data()), image.byte_size()),
        m_source_staging);
    if (!source) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureArrayBuffer::submit_layer failed to upload a {}x{} image", image.width, image.height);
        return false;
    }

    return submit_layer(layer, source);
}

bool TextureArrayBuffer::submit_layer(uint32_t layer, const std::shared_ptr<Core::VKImage>& image)
{
    if (layer >= m_layers || !image) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureArrayBuffer::submit_layer needs a layer below {} and an image, got layer {}",
            m_layers, layer);
        return false;
    }

    const bool same_extent = image->get_width() == get_width() && image->get_height() == get_height();
    const auto fit = fit_for(layer);
    if (!same_extent && !fit) {
        return false;
    }

    enter_layer_mode();
    if (!m_layer_mode) {
        return false;
    }

    auto plan = Portal::Graphics::SamplerForge::plan_fit(
        image->get_width(), image->get_height(), get_width(), get_height(),
        same_extent ? Portal::Graphics::FitMode::STRETCH : *fit, m_filter);

    auto& loom = Portal::Graphics::get_texture_manager();
    bool stored = !plan.blits.empty();

    if (stored) {
        const vk::Rect2D& first = plan.blits.front().dst;
        const bool covers = plan.blits.size() == 1
            && first.offset.x == 0 && first.offset.y == 0
            && first.extent.width == get_width() && first.extent.height == get_height();
        if (!covers) {
            clear_layer(layer);
        }
    }

    for (auto& blit : plan.blits) {
        blit.dst_layer = layer;
        stored = loom.blit_layer(image, get_texture(), blit) && stored;
    }

    return stored;
}

void TextureArrayBuffer::enable_layer_data()
{
    if (m_layer_data) {
        return;
    }

    const auto service = Registry::BackendRegistry::instance()
                             .get_service<Registry::Service::BufferService>();
    if (!service) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureArrayBuffer: layer data needs a buffer service");
        return;
    }

    m_layer_values.assign(static_cast<size_t>(m_layers) * 2, glm::vec4(0.0F));
    m_layer_data = std::make_shared<VKBuffer>(
        m_layer_values.size() * sizeof(glm::vec4),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);
    service->initialize_buffer(m_layer_data);

    std::memcpy(m_layer_data->get_mapped_ptr(), m_layer_values.data(),
        m_layer_values.size() * sizeof(glm::vec4));
    bind_layer_data();
}

void TextureArrayBuffer::set_layer_params(uint32_t layer, const glm::vec4& values)
{
    if (layer < m_layers) {
        write_layer_value(static_cast<size_t>(layer) * 2, values);
    }
}

void TextureArrayBuffer::set_layer_timing(uint32_t layer, const glm::vec4& values)
{
    if (layer < m_layers) {
        write_layer_value(static_cast<size_t>(layer) * 2 + 1, values);
    }
}

void TextureArrayBuffer::write_layer_value(size_t index, const glm::vec4& values)
{
    enable_layer_data();
    if (!m_layer_data) {
        return;
    }

    m_layer_values.at(index) = values;
    std::memcpy(static_cast<uint8_t*>(m_layer_data->get_mapped_ptr()) + index * sizeof(glm::vec4),
        &values, sizeof(glm::vec4));
}

void TextureArrayBuffer::bind_layer_data()
{
    if (const auto processor = get_render_processor(); processor && m_layer_data) {
        processor->bind_buffer("layerData", m_layer_data);
    }
}

void TextureArrayBuffer::clear_layer(uint32_t layer)
{
    if (m_blank.size() != m_layer_bytes) {
        m_blank.assign(m_layer_bytes, 0);
    }
    if (!m_layer_staging) {
        m_layer_staging = create_image_staging_buffer(m_layer_bytes);
    }

    Portal::Graphics::get_texture_manager().upload_layer(
        get_texture(), layer, m_blank.data(), m_blank.size(), m_layer_staging);
}

std::optional<Portal::Graphics::FitMode> TextureArrayBuffer::fit_for(uint32_t layer) const
{
    const auto found = m_layer_fit.find(layer);
    return found != m_layer_fit.end() ? found->second : m_fit;
}

void TextureArrayBuffer::set_weight(uint32_t layer, float weight)
{
    if (layer >= m_layers || layer >= m_push.weights.size()) {
        return;
    }

    m_push.weights.at(layer) = weight;
    flush_push();
}

void TextureArrayBuffer::set_weights(std::span<const float> weights)
{
    const auto count = std::min<size_t>({ weights.size(), m_layers, m_push.weights.size() });
    std::copy_n(weights.begin(), count, m_push.weights.begin());
    flush_push();
}

void TextureArrayBuffer::set_mode(uint32_t mode)
{
    m_push.mode = mode;
    flush_push();
}

void TextureArrayBuffer::set_push_constants(const void* data, size_t size)
{
    if (!data || size == 0) {
        m_custom_push.clear();
    } else {
        const auto* bytes = static_cast<const uint8_t*>(data);
        m_custom_push.assign(bytes, bytes + size);
    }

    if (const auto processor = get_render_processor()) {
        processor->set_push_constant_size(m_custom_push.empty() ? sizeof(Push) : m_custom_push.size());
    }
    flush_push();
}

void TextureArrayBuffer::ensure_array()
{
    if (has_texture()) {
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();
    auto image = loom.create_2d_array(get_width(), get_height(), m_layers, get_format());
    if (!image) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureArrayBuffer: failed to create a {} layer array image", m_layers);
        return;
    }

    set_gpu_texture(std::move(image));
}

void TextureArrayBuffer::enter_layer_mode()
{
    if (m_layer_mode) {
        return;
    }

    ensure_array();
    if (!has_texture()) {
        return;
    }

    auto& storage = get_pixel_data_mutable();
    const size_t total = m_layer_bytes * m_layers;
    const auto staging = create_image_staging_buffer(total);
    if (!staging) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureArrayBuffer: failed to allocate staging for {} bytes", total);
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();
    if (storage.size() == total) {
        loom.upload_data(get_texture(), storage.data(), total, staging);
    } else {
        const std::vector<uint8_t> blank(total, 0);
        loom.upload_data(get_texture(), blank.data(), total, staging);
    }

    std::vector<uint8_t>().swap(storage);
    m_layer_mode = true;
}

void TextureArrayBuffer::flush_push()
{
    const auto processor = get_render_processor();
    if (!processor) {
        return;
    }

    if (m_custom_push.empty()) {
        processor->set_push_constant_data(m_push);
    } else {
        processor->set_push_constant_data_raw(m_custom_push.data(), m_custom_push.size());
    }
}

}
