#pragma once

#include "Influence.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
class RenderProcessor;
}

namespace MayaFlux::Kinesis {
struct SurroundRadiance;
}

namespace MayaFlux::Nexus {

/**
 * @class InfluenceList
 * @brief Several influences for one draw, and the surroundings, for the lit shaders.
 *
 * Every lit fragment shader adds the influence pushed to it, if any, to those of the
 * list a renderer was given, and the surroundings. Each target() takes a place of its
 * own, which is emptied and freed when the last copy of the target goes, such as when
 * its Emitter is destroyed. Any number of renderers can read one list.
 *
 * A target may be called from any thread. A draw already running can read a block
 * half written, for that frame only.
 *
 * @code
 * auto lamps = std::make_shared<Nexus::InfluenceList>();
 * buffer->setup_rendering({ .target_window = window, .fragment_shader = "mesh_network_lit.frag.spv" });
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
     * @brief Give a renderer the list and the surroundings. Call before it first draws.
     * @param renderer A processor with a lit fragment shader. Ignored if null.
     */
    void bind(const std::shared_ptr<Buffers::RenderProcessor>& renderer) const;

    /**
     * @brief Give a renderer the surroundings and lobe table but not the list,
     *        such as one lit by a single pushed influence.
     * @param renderer A processor with a lit fragment shader. Ignored if null.
     */
    void bind_surround(const std::shared_ptr<Buffers::RenderProcessor>& renderer) const;

    /** @brief A target writing the next free place, or one that does nothing when the list is full. */
    [[nodiscard]] Influence<InfluenceBlock>::Target target() const;

    /**
     * @brief Light every surface from all around, as prefilter_surround() prepared it.
     * @param surround Ignored with a warning if a level or the table has the wrong size.
     * @param scale    Multiplies its radiance.
     *
     * @code
     * const auto sky = Kinesis::prefilter_surround({ pixels, width, height, channels });
     * lamps->set_surround(sky);
     * @endcode
     */
    void set_surround(const Kinesis::SurroundRadiance& surround, float scale = 1.0F) const;

    /**
     * @brief Give area lights exact shaped highlights.
     * @param lobes Kinesis::fit_transformed_cosines() at its default size; others are ignored with a warning.
     *
     * @code
     * lamps->set_lobes(Kinesis::fit_transformed_cosines());
     * @endcode
     */
    void set_lobes(std::span<const glm::vec4> lobes) const;

    /** @brief Return area lights to their nearest point highlights. */
    void clear_lobes() const;

    /** @brief Take the surroundings away. */
    void clear_surround() const;

    [[nodiscard]] uint32_t capacity() const noexcept;

    /** @brief The storage buffer: a count and three unused words, then the blocks. */
    [[nodiscard]] const std::shared_ptr<Buffers::VKBuffer>& buffer() const noexcept;

private:
    struct Places;
    std::shared_ptr<Places> m_places;
};

} // namespace MayaFlux::Nexus
