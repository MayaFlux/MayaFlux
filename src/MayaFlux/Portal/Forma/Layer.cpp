#include "Layer.hpp"

#include <ranges>

namespace MayaFlux::Portal::Forma {

// =============================================================================
// Element registration
// =============================================================================

uint32_t Layer::add(Element element)
{
    element.id = m_next_id++;
    m_elements.push_back(std::move(element));
    return m_elements.back().id;
}

bool Layer::remove(uint32_t id)
{
    auto it = std::ranges::find_if(m_elements,
        [id](const Element& e) { return e.id == id; });
    if (it == m_elements.end())
        return false;
    m_elements.erase(it);
    return true;
}

void Layer::clear()
{
    m_elements.clear();
}

// =============================================================================
// Element mutation
// =============================================================================

bool Layer::set_bounds(uint32_t id, Kinesis::AABB2D bounds)
{
    if (auto* el = get(id)) {
        el->bounds_hint = bounds;
        return true;
    }
    return false;
}

bool Layer::set_contains(uint32_t id, std::function<bool(glm::vec2)> fn)
{
    if (auto* el = get(id)) {
        el->contains = std::move(fn);
        return true;
    }
    return false;
}

bool Layer::set_interactive(uint32_t id, bool interactive)
{
    if (auto* el = get(id)) {
        el->interactive = interactive;
        return true;
    }
    return false;
}

bool Layer::set_visible(uint32_t id, bool visible)
{
    if (auto* el = get(id)) {
        el->visible = visible;
        return true;
    }
    return false;
}

bool Layer::bring_to_front(uint32_t id)
{
    auto it = std::ranges::find_if(m_elements,
        [id](const Element& e) { return e.id == id; });
    if (it == m_elements.end())
        return false;
    std::rotate(it, it + 1, m_elements.end());
    return true;
}

bool Layer::send_to_back(uint32_t id)
{
    auto it = std::ranges::find_if(m_elements,
        [id](const Element& e) { return e.id == id; });
    if (it == m_elements.end())
        return false;
    std::rotate(m_elements.begin(), it, it + 1);
    return true;
}

// =============================================================================
// Spatial queries
// =============================================================================

std::optional<uint32_t> Layer::hit_test(glm::vec2 ndc) const
{
    for (const auto& m_element : std::views::reverse(m_elements)) {
        if (test_element(m_element, ndc))
            return m_element.id;
    }
    return std::nullopt;
}

std::vector<uint32_t> Layer::hit_test_all(glm::vec2 ndc) const
{
    std::vector<uint32_t> result;
    for (const auto& m_element : std::views::reverse(m_elements)) {
        if (test_element(m_element, ndc))
            result.push_back(m_element.id);
    }
    return result;
}

std::optional<uint32_t> Layer::hit_test(
    double px, double py, uint32_t win_w, uint32_t win_h) const
{
    return hit_test(to_ndc(px, py, win_w, win_h));
}

std::vector<uint32_t> Layer::hit_test_all(
    double px, double py, uint32_t win_w, uint32_t win_h) const
{
    return hit_test_all(to_ndc(px, py, win_w, win_h));
}

// =============================================================================
// Introspection
// =============================================================================

const Element* Layer::get(uint32_t id) const
{
    auto it = std::ranges::find_if(m_elements,
        [id](const Element& e) { return e.id == id; });
    return it != m_elements.end() ? &*it : nullptr;
}

Element* Layer::get(uint32_t id)
{
    auto it = std::ranges::find_if(m_elements,
        [id](const Element& e) { return e.id == id; });
    return it != m_elements.end() ? &*it : nullptr;
}

// =============================================================================
// Internal
// =============================================================================

glm::vec2 Layer::to_ndc(
    double px, double py, uint32_t win_w, uint32_t win_h) noexcept
{
    return {
        (static_cast<float>(px) / static_cast<float>(win_w)) * 2.0F - 1.0F,
        1.0F - (static_cast<float>(py) / static_cast<float>(win_h)) * 2.0F
    };
}

bool Layer::test_element(const Element& el, glm::vec2 ndc) noexcept
{
    if (!el.interactive || !el.visible)
        return false;
    if (el.bounds_hint && !el.bounds_hint->contains(ndc))
        return false;
    if (el.contains)
        return el.contains(ndc);
    return el.bounds_hint.has_value();
}

} // namespace MayaFlux::Portal::Forma
