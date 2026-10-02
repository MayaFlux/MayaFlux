#include "Chimera.hpp"

#include "BufferPipeline.hpp"
#include "Tasks.hpp"

#include "MayaFlux/Buffers/Textures/TextureArrayBuffer.hpp"
#include "MayaFlux/IO/Image/ImageReader.hpp"
#include "MayaFlux/Kakshya/Source/DynamicVideoStream.hpp"
#include "MayaFlux/Kakshya/Utils/PixelStorage.hpp"
#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

void Chimera::State::halt()
{
    if (task && scheduler) {
        scheduler->cancel_task(task);
    }
    task.reset();
}

size_t Chimera::layer_count() const
{
    return m_state ? m_state->layers.size() : 0;
}

void Chimera::level(size_t layer, double weight)
{
    if (m_state) {
        m_state->buffer->set_weight(static_cast<uint32_t>(layer), static_cast<float>(weight));
    }
}

void Chimera::params(size_t layer, const glm::vec4& values)
{
    if (m_state) {
        m_state->buffer->set_layer_params(static_cast<uint32_t>(layer), values);
    }
}

void Chimera::speed(size_t layer, double ratio)
{
    if (m_state) {
        *m_state->layers.at(layer).ratio = ratio;
    }
}

void Chimera::cut(size_t layer, double seconds)
{
    if (!m_state) {
        return;
    }

    auto& target = m_state->layers.at(layer);
    if (!target.ring) {
        return;
    }

    if (target.lag) {
        const double elapsed = std::max(
            static_cast<double>(m_state->ticks) / m_state->frame_rate - target.delay, 0.0);
        target.lag_offset = seconds - (*target.lag)(elapsed);
    } else {
        target.position = static_cast<double>(target.ring->get_write_head())
            - seconds * target.ring->get_frame_rate();
        target.entered = true;
    }
}

void Chimera::set(size_t layer, Kakshya::ImageData image)
{
    if (!m_state) {
        return;
    }

    auto& target = m_state->layers.at(layer);
    target.ring.reset();
    target.image.reset();
    target.picture = std::make_shared<const Kakshya::ImageData>(std::move(image));
    target.dirty = true;
}

void Chimera::set(size_t layer, std::shared_ptr<Core::VKImage> image)
{
    if (!m_state) {
        return;
    }

    auto& target = m_state->layers.at(layer);
    target.ring.reset();
    target.picture.reset();
    target.image = std::move(image);
}

void Chimera::set(size_t layer, std::shared_ptr<Kakshya::DynamicVideoStream> ring)
{
    if (!m_state) {
        return;
    }

    auto& target = m_state->layers.at(layer);
    target.image.reset();
    target.picture.reset();
    target.ring = std::move(ring);
    target.entered = false;
}

void Chimera::stop()
{
    if (m_state) {
        m_state->halt();
    }
}

void Chimera::stamp(State& state, uint32_t index, const glm::vec4& timing)
{
    if (state.buffer->has_layer_data()) {
        state.buffer->set_layer_timing(index, timing);
    }
}

void Chimera::tick(State& state)
{
    const uint64_t frame = state.ticks++;
    const double now = static_cast<double>(frame) / state.frame_rate;

    uint32_t index = 0;
    for (auto& layer : state.layers) {
        const uint32_t current = index++;

        if (now < layer.delay) {
            continue;
        }

        const auto age = static_cast<float>(now - layer.delay);

        if (layer.picture) {
            if (layer.dirty && state.buffer->submit_layer(current, *layer.picture)) {
                layer.dirty = false;
                stamp(state, current, glm::vec4(age, 0.0F, 0.0F, 1.0F));
            }
            continue;
        }

        if (frame % layer.interval != 0) {
            continue;
        }

        if (layer.image) {
            if (state.buffer->submit_layer(current, layer.image)) {
                stamp(state, current, glm::vec4(age, 0.0F, 0.0F, 1.0F));
            }
        } else if (layer.ring) {
            feed(state, layer, current, now - layer.delay);
        }
    }
}

void Chimera::feed(State& state, Layer& layer, uint32_t index, double elapsed)
{
    const auto& ring = layer.ring;
    const uint64_t head = ring->get_write_head();
    if (head < 3) {
        return;
    }

    const double rate = ring->get_frame_rate();
    const double latest = static_cast<double>(head) - 2.0;

    if (layer.lag) {
        const double behind = ((*layer.lag)(elapsed) + layer.lag_offset) * rate;
        layer.position = static_cast<double>(head) - std::max(behind, 2.0);
    } else if (!layer.entered) {
        layer.position = latest;
    } else {
        layer.position = std::min(layer.position + *layer.ratio * rate / state.frame_rate, latest);
    }
    layer.entered = true;

    const double whole = std::max(std::floor(layer.position), 0.0);
    const auto frame = static_cast<uint64_t>(whole);
    if (!ring->is_frame_available(frame)) {
        if (!layer.missed) {
            MF_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "Chimera layer {} is outside the ring, holding its last frame", index);
            layer.missed = true;
        }
        return;
    }
    layer.missed = false;

    std::span<const uint8_t> pixels = ring->get_frame_pixels(frame);
    if (pixels.empty()) {
        return;
    }

    const double fraction = layer.position - whole;
    if (layer.smooth && fraction > 0.0 && ring->is_frame_available(frame + 1)) {
        const auto next = ring->get_frame_pixels(frame + 1);
        layer.scratch.resize(pixels.size());
        if (!next.empty()
            && Kakshya::blend_frames(ring->get_format(), pixels, next, layer.scratch.data(), fraction)) {
            pixels = std::span<const uint8_t>(layer.scratch);
        }
    }

    const bool same_extent = ring->get_width() == state.buffer->get_width()
        && ring->get_height() == state.buffer->get_height()
        && ring->get_format() == state.buffer->get_format();

    bool stored = false;
    if (same_extent) {
        stored = state.buffer->submit_layer(index, pixels);
    } else if (auto image = Kakshya::ImageData::from_bytes(
                   ring->get_width(), ring->get_height(),
                   Portal::Graphics::TextureLoom::get_channel_count(ring->get_format()),
                   ring->get_format(), pixels)) {
        stored = state.buffer->submit_layer(index, *image);
    }

    if (stored) {
        stamp(state, index,
            glm::vec4(static_cast<float>(elapsed), static_cast<float>(static_cast<double>(head) - layer.position),
                static_cast<float>(layer.position), 1.0F));
    }
}

ChimeraBuilder::ChimeraBuilder(
    std::shared_ptr<Buffers::TextureArrayBuffer> buffer,
    Vruta::TaskScheduler& scheduler,
    std::shared_ptr<BufferPipeline> pipeline)
    : m_buffer(std::move(buffer))
    , m_scheduler(scheduler)
    , m_pipeline(std::move(pipeline))
{
}

ChimeraBuilder& ChimeraBuilder::use_pipeline(std::shared_ptr<BufferPipeline> pipeline)
{
    m_pipeline = std::move(pipeline);
    m_recording = false;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::record(BufferOperation&& operation)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ChimeraBuilder::record needs a pipeline");
        return *this;
    }

    *m_pipeline >> std::move(operation);
    m_recording = true;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::render(Portal::Graphics::RenderConfig config)
{
    m_render = std::move(config);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::layer_data()
{
    if (m_buffer) {
        m_buffer->enable_layer_data();
    }
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from_pipeline()
{
    auto& target = current();
    target.ring.reset();
    target.image.reset();
    target.picture.reset();
    target.piped = true;
    return *this;
}

Chimera::Layer& ChimeraBuilder::current()
{
    if (m_layers.empty()) {
        m_layers.emplace_back();
    }
    return m_layers.back();
}

ChimeraBuilder& ChimeraBuilder::layer()
{
    m_layers.emplace_back();
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from(std::shared_ptr<Kakshya::DynamicVideoStream> ring)
{
    auto& target = current();
    target.image.reset();
    target.picture.reset();
    target.piped = false;
    target.ring = std::move(ring);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from(std::shared_ptr<Core::VKImage> image)
{
    auto& target = current();
    target.ring.reset();
    target.picture.reset();
    target.piped = false;
    target.image = std::move(image);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from(Kakshya::ImageData image)
{
    auto& target = current();
    target.ring.reset();
    target.image.reset();
    target.piped = false;
    target.picture = std::make_shared<const Kakshya::ImageData>(std::move(image));
    target.dirty = true;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from(const std::string& path)
{
    if (auto loaded = IO::ImageReader::load(path, 4)) {
        return from(std::move(*loaded));
    }

    MF_ERROR(Journal::Component::Kriya, Journal::Context::FileIO,
        "ChimeraBuilder::from could not load '{}'", path);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::enters_after(double seconds)
{
    current().delay = std::max(seconds, 0.0);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::lag(double seconds)
{
    return lag(Kinesis::constant<double, double>(seconds));
}

ChimeraBuilder& ChimeraBuilder::lag(Kinesis::TimeMap seconds)
{
    current().lag = std::make_shared<const Kinesis::TimeMap>(std::move(seconds));
    return *this;
}

ChimeraBuilder& ChimeraBuilder::speed(double ratio)
{
    *current().ratio = ratio;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::smooth(bool enable)
{
    current().smooth = enable;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::every_n_frames(uint32_t frames)
{
    current().interval = std::max(frames, 1U);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::fit(Portal::Graphics::FitMode mode)
{
    current().fit = mode;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::params(const glm::vec4& values)
{
    current().params = values;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::level(double weight)
{
    current().level = static_cast<float>(weight);
    return *this;
}

Chimera ChimeraBuilder::start()
{
    Chimera set;

    if (!m_buffer || m_layers.empty()) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ChimeraBuilder::start needs a buffer and at least one layer");
        return set;
    }

    if (m_layers.size() > m_buffer->get_layer_count()) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ChimeraBuilder::start declared {} layers, the buffer holds {}",
            m_layers.size(), m_buffer->get_layer_count());
        return set;
    }

    for (auto& layer : m_layers) {
        if (!layer.piped) {
            continue;
        }

        layer.ring = m_pipeline ? m_pipeline->get_graphics_stream() : nullptr;
        if (!layer.ring) {
            MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
                "ChimeraBuilder::start found no stream in the pipeline for from_pipeline");
            return set;
        }
    }

    auto state = std::make_shared<Chimera::State>();

    state->buffer = m_buffer;
    state->pipeline = m_pipeline;
    state->scheduler = &m_scheduler;
    state->layers = m_layers;
    state->frame_rate = std::max(
        static_cast<double>(m_scheduler.get_rate(Vruta::ProcessingToken::FRAME_ACCURATE)), 1.0);

    uint32_t index = 0;
    for (const auto& layer : state->layers) {
        m_buffer->set_weight(index, layer.level);
        if (layer.fit) {
            m_buffer->set_layer_fit(index, layer.fit);
        }
        if (layer.params) {
            m_buffer->set_layer_params(index, *layer.params);
        }
        ++index;
    }

    if (m_render) {
        m_buffer->setup_rendering(*m_render);
        m_render.reset();
    }

    const std::weak_ptr<Chimera::State> weak = state;
    state->task = metro(1.0 / state->frame_rate, [weak]() {
        if (const auto running = weak.lock()) {
            Chimera::tick(*running);
        }
    },
        Vruta::ProcessingToken::FRAME_ACCURATE);

    m_scheduler.add_task(state->task, "Chimera_" + std::to_string(m_scheduler.get_next_task_id()), false);

    if (m_recording) {
        m_pipeline->execute_frame_rate();
        m_recording = false;
    }

    set.m_guard = std::shared_ptr<void>(nullptr, [state](void*) { state->halt(); });
    set.m_state = std::move(state);
    return set;
}

} // namespace MayaFlux::Kriya
