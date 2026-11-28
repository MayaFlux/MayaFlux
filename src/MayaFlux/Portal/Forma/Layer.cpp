#include "Layer.hpp"

#include "LocalPoint.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Kinesis/Spatial/HitTest.hpp"
#include "MayaFlux/Portal/Graphics/ImageTarget.hpp"

namespace MayaFlux::Portal::Forma {

// =============================================================================
// Element registration
// =============================================================================

Layer::Slot Layer::add(Element element)
{
    element.id = m_next_id++;
    m_index[element.id] = m_elements.size();
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
            el->buffer.retire();
        std::erase_if(m_elements, [cid](const Element& e) { return e.id == cid; });
        m_index_stale = true;
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
    for (auto& el : m_elements) {
        if (el.buffer)
            el.buffer.retire();
    }
    m_batch.reset();
    m_batches.clear();
    m_image_batches.clear();
    m_elements.clear();
    m_index.clear();
    m_index_stale = false;
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

bool Layer::set_clip(uint32_t id, std::optional<Kinesis::AABB2D> clip)
{
    if (auto* el = get(id)) {
        el->clip = clip;
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
    if (!get(id))
        return false;

    for (uint32_t member : closure(id)) {
        auto* el = get(member);
        if (!el)
            continue;

        el->visible = visible;
        el->buffer.set_visible(visible);
    }
    return true;
}

void Layer::set_all_visible(bool visible)
{
    for (const auto& el : m_elements) {
        set_visible(el.id, visible);
    }
}

bool Layer::bring_to_front(uint32_t id)
{
    if (!get(id))
        return false;

    const std::vector<uint32_t> ids = closure(id);

    auto raise = [this](uint32_t target) {
        auto it = std::ranges::find_if(m_elements,
            [target](const Element& e) { return e.id == target; });
        if (it == m_elements.end())
            return;
        std::rotate(it, it + 1, m_elements.end());
        m_index_stale = true;

        if (it = m_elements.end() - 1; it->buffer)
            it->buffer.bring_to_front(++m_next_front_priority);
    };

    for (size_t i = 1; i < ids.size(); ++i)
        raise(ids[i]);
    raise(ids.front());

    return true;
}

bool Layer::send_to_back(uint32_t id)
{
    if (!get(id))
        return false;

    const std::vector<uint32_t> ids = closure(id);

    auto lower = [this](uint32_t target) {
        auto it = std::ranges::find_if(m_elements,
            [target](const Element& e) { return e.id == target; });
        if (it == m_elements.end())
            return;
        std::rotate(m_elements.begin(), it, it + 1);
        m_index_stale = true;

        if (auto& el = m_elements.front(); el.buffer)
            el.buffer.send_to_back(--m_next_back_priority);
    };

    for (size_t i = 1; i < ids.size(); ++i)
        lower(ids[i]);
    lower(ids.front());

    return true;
}

// =============================================================================
// Spatial queries
// =============================================================================

std::optional<std::pair<uint32_t, float>> Layer::topmost(glm::vec2 screen) const
{
    std::optional<std::pair<uint32_t, float>> nearest;
    const auto viewed = unprojected(screen);

    for (const auto& element : std::views::reverse(m_elements)) {
        const auto depth = test_element(element, screen, viewed);
        if (!depth)
            continue;
        if (!element.to_local && !placed())
            return std::pair { element.id, std::numeric_limits<float>::lowest() };
        if (!nearest || *depth < nearest->second)
            nearest.emplace(element.id, *depth);
    }
    return nearest;
}

std::optional<uint32_t> Layer::hit_test(glm::vec2 ndc) const
{
    const auto found = topmost(ndc);
    if (!found)
        return std::nullopt;
    return found->first;
}

std::optional<float> Layer::hit_depth(glm::vec2 ndc) const
{
    const auto found = topmost(ndc);
    if (!found)
        return std::nullopt;
    return found->second;
}

std::vector<uint32_t> Layer::hit_test_all(glm::vec2 ndc) const
{
    std::vector<uint32_t> result;
    std::vector<std::pair<float, uint32_t>> ranked;
    const auto viewed = unprojected(ndc);

    for (const auto& element : std::views::reverse(m_elements)) {
        const auto depth = test_element(element, ndc, viewed);
        if (!depth)
            continue;
        if (element.to_local || placed())
            ranked.emplace_back(*depth, element.id);
        else
            result.push_back(element.id);
    }

    std::ranges::stable_sort(ranked, {}, &std::pair<float, uint32_t>::first);
    for (const auto& [depth, id] : ranked)
        result.push_back(id);

    return result;
}

std::optional<glm::vec2> Layer::local_point(uint32_t id, glm::vec2 screen) const
{
    const Element* element = get(id);
    if (!element)
        return std::nullopt;

    const auto local = map_to_local(*element, screen, element->to_local ? std::nullopt : unprojected(screen));
    if (!local)
        return std::nullopt;

    return glm::vec2(*local);
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
// View
// =========================================================================

void Layer::set_view(const std::optional<Kinesis::ViewTransform>& view)
{
    m_view = view;
    if (placed()) {
        m_place_view = nullptr;
        m_placement = nullptr;
        m_placed_to_local = nullptr;
        for (auto& batch : m_batches)
            configure(batch);
    }
    for (const auto& [target, buffer] : m_image_batches) {
        if (target)
            target->request_redraw();
    }
}

glm::vec2 Layer::from_view(glm::vec2 screen) const
{
    constexpr float k_nowhere = 1.0e9F;
    const auto met = unprojected(screen);
    return met ? glm::vec2(*met) : glm::vec2(k_nowhere);
}

// =========================================================================
// Placement
// =========================================================================

namespace {

    constexpr uint32_t k_clip_constant = 2;
    constexpr float k_distance_scale = 1000.0F;
    constexpr float k_farthest = 1.0e12F;

}

void Layer::set_placement(
    std::function<Kinesis::ViewTransform()> view,
    std::function<glm::mat4()> placement)
{
    if (view && placement) {
        m_view.reset();
        m_placed_to_local = plane_to_local(view, placement);
        m_place_view = std::move(view);
        m_placement = std::move(placement);
    } else {
        m_placed_to_local = nullptr;
        m_place_view = nullptr;
        m_placement = nullptr;
    }

    for (auto& batch : m_batches)
        configure(batch);

    for (const auto& [target, buffer] : m_image_batches) {
        if (target)
            target->request_redraw();
    }
}

Kinesis::ViewTransform Layer::drawn_view() const
{
    if (!placed())
        return m_view.value_or(Kinesis::ViewTransform {});

    const Kinesis::ViewTransform seen = m_place_view();
    const Kinesis::ViewTransform composed { .view = seen.view * m_placement(), .projection = seen.projection };

    const glm::mat4 back = glm::inverse(composed.view);
    const bool perspective = composed.projection[2][3] != 0.0F;
    const bool front = perspective ? back[3].z > 0.0F : back[2].z > 0.0F;

    if (!front)
        return { .view = composed.view, .projection = glm::mat4(0.0F) };

    return composed;
}

int64_t Layer::placed_rank() const
{
    if (!placed())
        return 0;

    const float distance = -(m_place_view().view * m_placement())[3].z;
    return -static_cast<int64_t>(std::clamp(distance * k_distance_scale, 0.0F, k_farthest));
}

void Layer::follow(const std::shared_ptr<Buffers::FormaBuffer>& batch)
{
    if (!batch)
        return;

    std::erase_if(m_batches, [](const Batch& held) { return held.buffer.expired(); });
    m_batches.push_back({ .buffer = batch,
        .clip = std::nullopt,
        .flat_band = batch->get_render_config().draw_band });
    configure(m_batches.back());
}

void Layer::clip_batch(const std::shared_ptr<Buffers::FormaBuffer>& batch, const Kinesis::AABB2D& rect)
{
    if (!batch)
        return;

    const auto found = std::ranges::find_if(m_batches,
        [&batch](const Batch& held) { return held.buffer.lock() == batch; });
    if (found == m_batches.end()) {
        if (const auto processor = batch->get_render_processor())
            processor->set_scissor(Kinesis::Scissor::from(rect));
        return;
    }

    found->clip = rect;
    configure(*found);
}

void Layer::configure(Batch& batch) const
{
    const auto buffer = batch.buffer.lock();
    const auto processor = buffer ? buffer->get_render_processor() : nullptr;
    if (!processor)
        return;

    const bool on_plane = placed();
    const bool was_on_plane = std::exchange(batch.on_plane, on_plane);

    if (on_plane || was_on_plane) {
        auto config = buffer->get_render_config();
        if (on_plane) {
            config.draw_band = Portal::Graphics::DrawBand::TRANSLUCENT;
            config.depth_test = Portal::Graphics::CompareOp::LESS_OR_EQUAL;
            config.depth_write = false;
            buffer->set_draw_rank_source([layer = weak_from_this()]() -> int64_t {
                const auto held = layer.lock();
                return held ? held->placed_rank() : 0;
            });
        } else {
            config.draw_band = batch.flat_band;
            config.depth_test.reset();
            config.depth_write = true;
            buffer->set_draw_rank(0);
        }
        buffer->set_render_config(config);
        processor->set_depth_stamp(on_plane);
    }

    if (!batch.clip)
        return;

    processor->set_scissor(on_plane ? std::nullopt
                                    : std::optional<Kinesis::Scissor>(Kinesis::Scissor::from(*batch.clip)));

    if (!on_plane && !was_on_plane)
        return;

    const glm::vec4 rect = on_plane
        ? glm::vec4(batch.clip->min, batch.clip->max)
        : glm::vec4(0.0F);
    for (uint32_t i = 0; i < 4; ++i)
        processor->set_specialization_constant(k_clip_constant + i, std::bit_cast<uint32_t>(rect[static_cast<int>(i)]));
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
    r->buffer.set_visible(p->visible);

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

void Layer::reindex() const
{
    m_index.clear();
    for (size_t i = 0; i < m_elements.size(); ++i)
        m_index[m_elements[i].id] = i;
    m_index_stale = false;
}

const Element* Layer::get(uint32_t id) const
{
    if (m_index_stale)
        reindex();
    const auto it = m_index.find(id);
    return it != m_index.end() ? &m_elements[it->second] : nullptr;
}

Element* Layer::get(uint32_t id)
{
    return const_cast<Element*>(std::as_const(*this).get(id));
}

std::optional<uint32_t> Layer::find(std::string_view name) const
{
    if (name.empty())
        return std::nullopt;
    const auto found = std::ranges::find_if(m_elements,
        [name](const Element& element) { return element.name == name; });
    if (found == m_elements.end())
        return std::nullopt;
    return found->id;
}

// =============================================================================
// Internal
// =============================================================================

glm::vec2 Layer::to_ndc(double px, double py, uint32_t w, uint32_t h) noexcept
{
    auto ndc3 = Kinesis::to_ndc(px, py, w, h);
    return { ndc3.x, ndc3.y };
}

std::optional<glm::vec3> Layer::unprojected(glm::vec2 screen) const
{
    if (placed())
        return m_placed_to_local(screen);

    if (!m_view)
        return glm::vec3(screen, 0.0F);

    const auto met = Kinesis::view_plane_point(screen, *m_view);
    if (!met)
        return std::nullopt;

    return glm::vec3(*met, 0.0F);
}

std::optional<glm::vec3> Layer::map_to_local(
    const Element& el, glm::vec2 screen, const std::optional<glm::vec3>& viewed)
{
    if (el.to_local) {
        const auto local = el.to_local(screen);
        if (!local || !std::isfinite(local->x) || !std::isfinite(local->y) || !std::isfinite(local->z))
            return std::nullopt;
        return local;
    }

    return viewed;
}

std::optional<float> Layer::test_element(
    const Element& el, glm::vec2 screen, const std::optional<glm::vec3>& viewed)
{
    if (!el.interactive || !el.visible)
        return std::nullopt;

    const auto local = map_to_local(el, screen, viewed);
    if (!local || !el.hits(glm::vec2(*local)))
        return std::nullopt;

    return local->z;
}

} // namespace MayaFlux::Portal::Forma
