#include "Chimera.hpp"

#include "BufferPipeline.hpp"

#include "MayaFlux/Buffers/Textures/TextureArrayBuffer.hpp"
#include "MayaFlux/IO/Image/ImageReader.hpp"
#include "MayaFlux/Kakshya/Source/DynamicVideoStream.hpp"
#include "MayaFlux/Kakshya/Source/TextureCollection.hpp"
#include "MayaFlux/Kakshya/Utils/PixelStorage.hpp"
#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

void Chimera::State::halt()
{
    if (pipeline) {
        pipeline->end();
    }
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
    if (!target.ring && !target.collection) {
        return;
    }

    if (target.lag) {
        const double elapsed = std::max(
            static_cast<double>(m_state->ticks) / m_state->frame_rate - target.delay, 0.0);
        target.lag_offset = seconds - (*target.lag)(elapsed);
    } else if (target.ring) {
        target.position = static_cast<double>(target.ring->get_write_head())
            - seconds * target.ring->get_frame_rate();
        target.entered = true;
    } else {
        const double rate = target.collection_rate > 0.0 ? target.collection_rate : m_state->frame_rate;
        target.position = static_cast<double>(target.collection->get_write_head()) - seconds * rate;
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
    target.collection.reset();
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
    target.collection.reset();
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
    target.collection.reset();
    target.ring = std::move(ring);
    target.entered = false;
}

void Chimera::set(size_t layer, std::shared_ptr<Kakshya::TextureCollection> collection, double frame_rate)
{
    if (!m_state) {
        return;
    }

    auto& target = m_state->layers.at(layer);
    target.image.reset();
    target.picture.reset();
    target.ring.reset();
    target.collection = std::move(collection);
    target.collection_rate = frame_rate;
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

void Chimera::tick(State& state, uint64_t frame)
{
    state.ticks = frame + 1;
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
        } else if (layer.collection) {
            collect(state, layer, current, now - layer.delay);
        }
    }
}

void Chimera::place(const State& state, Layer& layer, uint64_t head, double rate, double nearest, double elapsed)
{
    const double latest = static_cast<double>(head) - nearest;

    if (layer.lag) {
        const double behind = ((*layer.lag)(elapsed) + layer.lag_offset) * rate;
        layer.position = static_cast<double>(head) - std::max(behind, nearest);
    } else if (!layer.entered) {
        layer.position = latest;
    } else {
        layer.position = std::min(layer.position + *layer.ratio * rate / state.frame_rate, latest);
    }
    layer.entered = true;
}

void Chimera::collect(State& state, Layer& layer, uint32_t index, double elapsed)
{
    const auto& collection = layer.collection;
    const uint64_t head = collection->get_write_head();
    if (head == 0) {
        return;
    }

    const double rate = layer.collection_rate > 0.0 ? layer.collection_rate : state.frame_rate;
    place(state, layer, head, rate, 1.0, elapsed);

    const auto frame = static_cast<uint64_t>(std::max(std::floor(layer.position), 0.0));
    const uint64_t ring = collection->get_spec().ring_layers;
    if (frame >= head || (ring > 0 && frame + ring < head)) {
        if (!layer.missed) {
            MF_WARN(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                "Chimera layer {} is outside the collection, holding its last frame", index);
            layer.missed = true;
        }
        return;
    }
    layer.missed = false;

    const auto slot = static_cast<uint32_t>(ring > 0 ? frame % ring : frame);
    if (state.buffer->submit_layer(index, collection->to_image(slot))) {
        stamp(state, index,
            glm::vec4(static_cast<float>(elapsed), static_cast<float>(static_cast<double>(head) - layer.position),
                static_cast<float>(layer.position), 1.0F));
    }
}

void Chimera::feed(State& state, Layer& layer, uint32_t index, double elapsed)
{
    const auto& ring = layer.ring;
    const uint64_t head = ring->get_write_head();
    if (head < 3) {
        return;
    }

    place(state, layer, head, ring->get_frame_rate(), 2.0, elapsed);

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
    std::shared_ptr<BufferPipeline> pipeline)
    : m_buffer(std::move(buffer))
    , m_pipeline(std::move(pipeline))
{
}

ChimeraBuilder& ChimeraBuilder::use_pipeline(std::shared_ptr<BufferPipeline> pipeline)
{
    m_pipeline = std::move(pipeline);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::record(BufferOperation&& operation)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ChimeraBuilder::record needs a pipeline");
        return *this;
    }

    m_pipeline >> std::move(operation);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::every(double seconds, std::function<void(Chimera&)> action)
{
    m_actions.push_back({ .seconds = seconds, .run = std::move(action) });
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
    target.collection.reset();
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
    target.collection.reset();
    target.piped = false;
    target.ring = std::move(ring);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from(std::shared_ptr<Kakshya::TextureCollection> collection, double frame_rate)
{
    auto& target = current();
    target.image.reset();
    target.picture.reset();
    target.ring.reset();
    target.piped = false;
    target.collection = std::move(collection);
    target.collection_rate = frame_rate;
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from(std::shared_ptr<Core::VKImage> image)
{
    auto& target = current();
    target.ring.reset();
    target.collection.reset();
    target.picture.reset();
    target.piped = false;
    target.image = std::move(image);
    return *this;
}

ChimeraBuilder& ChimeraBuilder::from(Kakshya::ImageData image)
{
    auto& target = current();
    target.ring.reset();
    target.collection.reset();
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

    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ChimeraBuilder::start needs a pipeline to feed the layers");
        return set;
    }

    for (auto& layer : m_layers) {
        if (!layer.piped) {
            continue;
        }

        layer.ring = m_pipeline->get_graphics_stream();
        if (!layer.ring) {
            MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
                "ChimeraBuilder::start found no stream in the pipeline for from_pipeline");
            return set;
        }
    }

    auto state = std::make_shared<Chimera::State>();

    state->buffer = m_buffer;
    state->pipeline = m_pipeline;
    state->layers = m_layers;
    state->frame_rate = std::max(static_cast<double>(Vruta::s_registered_frame_rate), 1.0);

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
    m_pipeline >> BufferOperation::dispatch_to(
        [weak](Kakshya::DataVariant&, uint32_t cycle) {
            if (const auto running = weak.lock()) {
                Chimera::tick(*running, cycle);
            }
        },
        Buffers::ProcessingToken::GRAPHICS_BACKEND);

    for (const auto& action : m_actions) {
        const auto cycles = static_cast<uint32_t>(std::max(std::round(action.seconds * state->frame_rate), 1.0));

        auto timed = BufferOperation::dispatch_to(
            [weak, run = action.run](Kakshya::DataVariant&, uint32_t cycle) {
                const auto running = weak.lock();
                if (!running || cycle == 0) {
                    return;
                }

                Chimera view;
                view.m_state = running;
                run(view);
            },
            Buffers::ProcessingToken::GRAPHICS_BACKEND);
        timed.every_n_cycles(cycles);

        m_pipeline >> std::move(timed);
    }

    m_pipeline->execute_frame_rate();

    set.m_guard = std::shared_ptr<void>(nullptr, [state](void*) { state->halt(); });
    set.m_state = std::move(state);
    return set;
}

} // namespace MayaFlux::Kriya
