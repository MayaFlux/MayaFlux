#include "Layer.hpp"

namespace MayaFlux::Portal::Forma {

// =============================================================================
// Element registration
// =============================================================================

Layer::Slot Layer::add(Element element)
{
    element.id = m_next_id++;
    m_elements.push_back(std::move(element));
    return Slot(*this, m_elements.back().id);
}

bool Layer::remove(uint32_t id)
{
    if (!get(id))
        return false;

    const std::vector<uint32_t> ids = closure(id);

    for (uint32_t cid : ids) {
        if (auto* el = get(cid); el && el->buffer)
            el->buffer->mark_for_removal();
        std::erase_if(m_elements, [cid](const Element& e) { return e.id == cid; });
    }

    for (uint32_t cid : ids)
        m_relations.erase(cid);

    for (auto& [primary_id, related] : m_relations) {
        for (uint32_t cid : ids)
            std::erase(related, cid);
    }
    std::erase_if(m_relations, [](const auto& kv) { return kv.second.empty(); });

    return true;
}

void Layer::clear()
{
    m_elements.clear();
    m_relations.clear();
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
    auto* root = get(id);
    if (!root)
        return false;

    root->visible = visible;
    if (root->buffer)
        root->buffer->mark_for_processing(visible);

    if (auto it = m_relations.find(id); it != m_relations.end()) {
        for (uint32_t rel_id : it->second)
            set_visible(rel_id, visible);
    }
    return true;
}

bool Layer::bring_to_front(uint32_t id)
{
    if (auto rit = m_relations.find(id); rit != m_relations.end()) {
        for (uint32_t rel_id : rit->second) {
            auto it = std::ranges::find_if(m_elements,
                [rel_id](const Element& e) { return e.id == rel_id; });
            if (it != m_elements.end())
                std::rotate(it, it + 1, m_elements.end());

            if (auto* rel_el = get(rel_id); rel_el && rel_el->buffer) {
                auto config = rel_el->buffer->get_render_config();
                config.draw_priority = ++m_next_front_priority;
                rel_el->buffer->set_render_config(config);
            }
        }
    }
    auto it = std::ranges::find_if(m_elements,
        [id](const Element& e) { return e.id == id; });
    if (it == m_elements.end())
        return false;
    std::rotate(it, it + 1, m_elements.end());

    if (auto* el = get(id); el && el->buffer) {
        auto config = el->buffer->get_render_config();
        config.draw_priority = ++m_next_front_priority;
        el->buffer->set_render_config(config);
    }

    return true;
}

bool Layer::send_to_back(uint32_t id)
{
    if (auto rit = m_relations.find(id); rit != m_relations.end()) {
        for (uint32_t rel_id : rit->second)
            send_to_back(rel_id);
    }

    auto it = std::ranges::find_if(m_elements,
        [id](const Element& e) { return e.id == id; });
    if (it == m_elements.end())
        return false;
    std::rotate(m_elements.begin(), it, it + 1);

    if (auto* el = get(id); el && el->buffer) {
        auto config = el->buffer->get_render_config();
        config.draw_priority = --m_next_back_priority;
        el->buffer->set_render_config(config);
    }

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

// =========================================================================
// Relations
// =========================================================================

bool Layer::relate(uint32_t primary_id, uint32_t related_id)
{
    auto* p = get(primary_id);
    auto* r = get(related_id);
    if (!p || !r || primary_id == related_id)
        return false;

    auto& rel = m_relations[primary_id];
    if (std::ranges::find(rel, related_id) == rel.end())
        rel.push_back(related_id);

    r->visible = p->visible;
    if (r->buffer)
        r->buffer->mark_for_processing(p->visible);

    return true;
}

bool Layer::unrelate(uint32_t primary_id, uint32_t related_id)
{
    auto it = m_relations.find(primary_id);
    if (it == m_relations.end())
        return false;
    auto& rel = it->second;
    auto rit = std::ranges::find(rel, related_id);
    if (rit == rel.end())
        return false;
    rel.erase(rit);
    if (rel.empty())
        m_relations.erase(it);
    return true;
}

std::vector<uint32_t> Layer::related_ids(uint32_t primary_id) const
{
    auto it = m_relations.find(primary_id);
    return it != m_relations.end() ? it->second : std::vector<uint32_t> {};
}

std::vector<uint32_t> Layer::closure(uint32_t id) const
{
    std::vector<uint32_t> result { id };

    for (size_t i = 0; i < result.size(); ++i) {
        auto it = m_relations.find(result[i]);
        if (it == m_relations.end())
            continue;
        for (uint32_t rel_id : it->second) {
            if (std::ranges::find(result, rel_id) == result.end())
                result.push_back(rel_id);
        }
    }

    return result;
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

glm::vec2 Layer::to_ndc(double px, double py, uint32_t w, uint32_t h) noexcept
{
    auto ndc3 = Kinesis::to_ndc(px, py, w, h);
    return { ndc3.x, ndc3.y };
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
