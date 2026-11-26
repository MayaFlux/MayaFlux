#pragma once

#include "Influence.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
class RenderProcessor;
}

namespace MayaFlux::Nexus {

/**
 * @class InfluenceList
 * @brief Several influences for one draw, where an InfluenceBlock push gives it one.
 *
 * One storage buffer of InfluenceBlocks. Each target() takes a place of its own, which
 * is emptied and freed when the last copy of the target goes, such as when its Emitter
 * is destroyed. Renderers drawn with a lit_list fragment shader, mesh_lit_list and the
 * rest, add up every place. Any number of renderers can read one list.
 *
 * A target may be called from any thread. A draw already running can read a block
 * half written, for that frame only.
 *
 * @code
 * auto lamps = std::make_shared<Nexus::InfluenceList>();
 * buffer->setup_rendering({ .target_window = window, .fragment_shader = "mesh_network_lit_list.frag.spv" });
 * lamps->bind(buffer->get_render_processor());
 *
 * red->set_influence<Nexus::InfluenceBlock>(Nexus::InfluenceBlock::from, lamps->target());
 * blue->set_influence<Nexus::InfluenceBlock>(Nexus::InfluenceBlock::from, lamps->target());
 * @endcode
 */
class MAYAFLUX_API InfluenceList {
public:
    /**
     * @param capacity Most influences held at once.
     * @throws std::runtime_error without a buffer service, so graphics must be up.
     */
    explicit InfluenceList(uint32_t capacity = 16);

    /**
     * @brief Give a renderer the list. Call before it first draws.
     * @param renderer A processor with a lit_list fragment shader. Ignored if null.
     */
    void bind(const std::shared_ptr<Buffers::RenderProcessor>& renderer) const;

    /** @brief A target writing the next free place, or one that does nothing when the list is full. */
    [[nodiscard]] Influence<InfluenceBlock>::Target target() const;

    [[nodiscard]] uint32_t capacity() const noexcept;

    /** @brief The storage buffer: a count and three unused words, then the blocks. */
    [[nodiscard]] const std::shared_ptr<Buffers::VKBuffer>& buffer() const noexcept;

private:
    struct Places;
    std::shared_ptr<Places> m_places;
};

} // namespace MayaFlux::Nexus
