#include "TextSlice.hpp"

#include "Atelier.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Portal/Graphics/ImageTarget.hpp"

namespace MayaFlux::Portal::Forma::internal {

namespace {

    std::vector<std::weak_ptr<TextSlice>>& live_slices()
    {
        static std::vector<std::weak_ptr<TextSlice>> slices;
        return slices;
    }

    bool& reflowing()
    {
        static bool active = false;
        return active;
    }

    void reflow_others(const TextSlice* origin, const Text::GlyphAtlas* atlas)
    {
        if (reflowing()) {
            return;
        }
        reflowing() = true;

        auto& slices = live_slices();
        std::erase_if(slices, [](const auto& slice) { return slice.expired(); });
        for (const auto& weak : slices) {
            if (auto slice = weak.lock();
                slice && slice.get() != origin && slice->run().atlas() == atlas && slice->run().stale()) {
                slice->refresh();
            }
        }

        reflowing() = false;
    }

} // namespace

TextSlice::TextSlice(FormaTarget target, Kinesis::AABB2D bounds, Text::PressParams params)
    : m_target(std::move(target))
    , m_bounds(bounds)
    , m_run(std::move(params))
{
}

std::shared_ptr<TextSlice> TextSlice::make(
    FormaTarget target,
    Kinesis::AABB2D bounds,
    Text::PressParams params,
    std::string_view text)
{
    auto slice = std::shared_ptr<TextSlice>(new TextSlice(std::move(target), bounds, std::move(params)));
    live_slices().push_back(slice);
    slice->set(text);
    return slice;
}

void TextSlice::set(std::string_view text)
{
    m_run.set(text);
    refresh();
}

void TextSlice::assign(std::string_view text, Text::PressParams params)
{
    m_run.assign(text, std::move(params));
    refresh();
}

void TextSlice::set_bounds(Kinesis::AABB2D bounds)
{
    m_bounds = bounds;
    refresh();
}

void TextSlice::set_tail(std::vector<Kakshya::MeshVertex> tail)
{
    m_tail = std::move(tail);
    refresh();
}

Text::GlyphMap TextSlice::map() const noexcept
{
    Kinesis::AABB2D bounds = m_bounds;

    const auto& buffer = m_target.buffer();
    const auto processor = buffer ? buffer->get_render_processor() : nullptr;
    if (processor
        && buffer->get_render_config().draw_band != Portal::Graphics::DrawBand::TRANSLUCENT) {
        glm::vec2 size { 0.0F };
        if (const auto window = processor->get_target_window()) {
            const auto& state = window->get_state();
            size = { static_cast<float>(state.current_width), static_cast<float>(state.current_height) };
        } else if (const auto image = processor->get_target_image()) {
            size = { static_cast<float>(image->width()), static_cast<float>(image->height()) };
        }

        if (size.x > 0.0F && size.y > 0.0F) {
            const glm::vec2 corner { bounds.min.x, bounds.max.y };
            const glm::vec2 whole = glm::round((corner + 1.0F) * 0.5F * size) / size * 2.0F - 1.0F;
            bounds = bounds.translated(whole - corner);
        }
    }

    return Text::GlyphMap::fit(bounds, m_run.extent());
}

bool TextSlice::alive() const
{
    const auto& buffer = m_target.buffer();
    if (!buffer || buffer->needs_removal()) {
        return false;
    }
    return m_target.slice() == 0 || buffer->has_slice(m_target.slice());
}

void TextSlice::refresh()
{
    Text::GlyphAtlas* atlas = m_run.atlas();
    if (!atlas || !alive()) {
        return;
    }

    auto vertices = m_run.vertices(map(), 0);
    vertices.insert(vertices.end(), m_tail.begin(), m_tail.end());

    const auto gpu = atelier().atlas_image(*atlas).sync(*atlas);

    const bool replaced = gpu && gpu != m_bound_image;
    const bool superseded = replaced && m_bound_image;
    if (replaced) {
        m_target.bind_texture(0, gpu);
        m_bound_image = gpu;
    }

    m_target.submit(vertices);

    if (superseded) {
        reflow_others(this, atlas);
    }
}

} // namespace MayaFlux::Portal::Forma::internal
