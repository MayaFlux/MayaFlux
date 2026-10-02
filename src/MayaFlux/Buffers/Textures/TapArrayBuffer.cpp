#include "TapArrayBuffer.hpp"

#include "TextureProcessor.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

TapArrayBuffer::TapArrayBuffer(
    uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    uint32_t layers)
    : TextureBuffer(width, height, format)
    , m_layers(std::clamp(layers, 1U, MAX_LAYERS))
    , m_layer_bytes(static_cast<size_t>(width) * height * Portal::Graphics::TextureLoom::get_bytes_per_pixel(format))
{
    if (layers != m_layers) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::Configuration,
            "TapArrayBuffer: {} layers requested, using {}", layers, m_layers);
    }

    RenderConfig config;
    config.vertex_shader = "texture.vert.spv";
    config.fragment_shader = "tap_array.frag.spv";
    config.default_texture_binding = "tapArray";
    config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP;
    set_default_render_config(config);

    m_push.layer_count = m_layers;
    m_push.weights.fill(1.0F);
}

void TapArrayBuffer::setup_processors(ProcessingToken token)
{
    TextureBuffer::setup_processors(token);
    ensure_array();

    if (const auto processor = get_texture_processor()) {
        processor->set_streaming_mode(true);
    }
}

void TapArrayBuffer::setup_rendering(const RenderConfig& config)
{
    ensure_array();
    TextureBuffer::setup_rendering(config);

    if (const auto processor = get_render_processor()) {
        processor->set_push_constant_size(m_custom_push.empty() ? sizeof(Push) : m_custom_push.size());
    }
    flush_push();
}

bool TapArrayBuffer::submit_layer(uint32_t layer, std::span<const uint8_t> pixels)
{
    if (layer >= m_layers || pixels.size() != m_layer_bytes) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TapArrayBuffer::submit_layer needs a layer below {} and {} bytes, got layer {} and {} bytes",
            m_layers, m_layer_bytes, layer, pixels.size());
        return false;
    }

    auto& storage = get_pixel_data_mutable();
    if (storage.size() != m_layer_bytes * m_layers) {
        storage.assign(m_layer_bytes * m_layers, 0);
    }

    std::memcpy(storage.data() + static_cast<size_t>(layer) * m_layer_bytes, pixels.data(), m_layer_bytes);
    mark_pixels_dirty();
    return true;
}

void TapArrayBuffer::set_weight(uint32_t layer, float weight)
{
    if (layer >= m_layers) {
        return;
    }

    m_push.weights[layer] = weight;
    flush_push();
}

void TapArrayBuffer::set_weights(std::span<const float> weights)
{
    const size_t count = std::min<size_t>(weights.size(), m_layers);
    std::copy_n(weights.begin(), count, m_push.weights.begin());
    flush_push();
}

void TapArrayBuffer::set_mode(uint32_t mode)
{
    m_push.mode = mode;
    flush_push();
}

void TapArrayBuffer::set_push_constants(const void* data, size_t size)
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

void TapArrayBuffer::ensure_array()
{
    if (has_texture()) {
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();
    auto image = loom.create_2d_array(get_width(), get_height(), m_layers, get_format());
    if (!image) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TapArrayBuffer: failed to create a {} layer array image", m_layers);
        return;
    }

    set_gpu_texture(std::move(image));
}

void TapArrayBuffer::flush_push()
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
