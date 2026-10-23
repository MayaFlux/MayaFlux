#include "Target.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

namespace MayaFlux::Portal::Forma {

FormaTarget::FormaTarget(std::shared_ptr<Buffers::FormaBuffer> buffer)
    : FormaTarget(std::move(buffer), 0)
{
}

FormaTarget::FormaTarget(std::shared_ptr<Buffers::FormaBuffer> buffer, SliceId slice)
{
    if (buffer) {
        m_state = std::make_shared<State>(State { .buffer = std::move(buffer), .slice = slice });
    }
}

Buffers::FormaBuffer* FormaTarget::operator->() const noexcept
{
    return m_state ? m_state->buffer.get() : nullptr;
}

Buffers::FormaBuffer& FormaTarget::operator*() const noexcept
{
    return *m_state->buffer;
}

const std::shared_ptr<Buffers::FormaBuffer>& FormaTarget::buffer() const noexcept
{
    static const std::shared_ptr<Buffers::FormaBuffer> none;
    return m_state ? m_state->buffer : none;
}

FormaTarget::SliceId FormaTarget::slice() const noexcept
{
    return m_state ? m_state->slice : 0;
}

void FormaTarget::submit(const std::vector<uint8_t>& bytes) const
{
    if (!m_state)
        return;

    if (m_state->slice != 0) {
        m_state->buffer->update_slice(m_state->slice, bytes);
        return;
    }
    m_state->buffer->submit(bytes);
}

void FormaTarget::bind_texture(uint32_t array_index, const std::shared_ptr<Core::VKImage>& image) const
{
    if (m_state)
        m_state->buffer->bind_texture(array_index, image);
}

void FormaTarget::set_visible(bool visible) const
{
    if (!m_state)
        return;

    if (m_state->slice != 0) {
        m_state->buffer->set_slice_visible(m_state->slice, visible);
        return;
    }

    if (auto render = m_state->buffer->get_render_processor())
        render->set_visible(visible, m_state->buffer);
}

void FormaTarget::bring_to_front(int64_t priority) const
{
    if (!m_state)
        return;

    if (m_state->slice != 0) {
        m_state->buffer->bring_slice_to_front(m_state->slice);
        return;
    }

    auto config = m_state->buffer->get_render_config();
    config.draw_priority = priority;
    m_state->buffer->set_render_config(config);
}

void FormaTarget::send_to_back(int64_t priority) const
{
    if (!m_state)
        return;

    if (m_state->slice != 0) {
        m_state->buffer->send_slice_to_back(m_state->slice);
        return;
    }

    auto config = m_state->buffer->get_render_config();
    config.draw_priority = priority;
    m_state->buffer->set_render_config(config);
}

void FormaTarget::retire() const
{
    if (!m_state)
        return;

    if (m_state->slice != 0) {
        m_state->buffer->remove_slice(m_state->slice);
        return;
    }
    m_state->buffer->mark_for_removal();
}

} // namespace MayaFlux::Portal::Forma
