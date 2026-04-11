#pragma once

#include "InfluenceContext.hpp"
#include "Sinks.hpp"

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Nexus {

/**
 * @class Emitter
 * @brief Object that acts on existing MayaFlux objects when committed.
 *
 * Constructed with only an influence function. Position is optional: call
 * @c set_position before registering with @c Fabric if spatial indexing is
 * required. Entities without a position are committed normally but are not
 * inserted into the spatial index.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's
 * lifetime. It is zero until the object has been registered.
 */
class Emitter {
public:
    using InfluenceFn = std::function<void(const InfluenceContext&)>;

    /**
     * @brief Construct with an influence function.
     * @param fn Called on every commit with the current context.
     */
    explicit Emitter(InfluenceFn fn)
        : m_fn(std::move(fn))
    {
    }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] const std::optional<glm::vec3>& position() const { return m_position; }

    /**
     * @brief Set the position, enabling spatial indexing for this object.
     * @param p World-space coordinates.
     */
    void set_position(const glm::vec3& p) { m_position = p; }

    /**
     * @brief Clear the position, removing this object from spatial queries.
     */
    void clear_position() { m_position.reset(); }

    /**
     * @brief Return the stable object id assigned by Fabric.
     */
    [[nodiscard]] uint32_t id() const { return m_id; }

    // =========================================================================
    // Output sinks
    // =========================================================================

    /** @brief Register an audio output on @p channel. */
    void sink_audio(Buffers::BufferManager& mgr, uint32_t channel)
    {
        add_audio_sink(m_audio_sinks, mgr, channel);
    }

    /** @brief Register an audio output on @p channel with a producer function. */
    void sink_audio(Buffers::BufferManager& mgr, uint32_t channel,
        std::function<Kakshya::DataVariant(const InfluenceContext&)> fn)
    {
        add_audio_sink(m_audio_sinks, mgr, channel, std::move(fn));
    }

    /** @brief Unregister the audio sink on @p channel. */
    void remove_audio_sink(Buffers::BufferManager& mgr, uint32_t channel)
    {
        Nexus::remove_audio_sink(m_audio_sinks, mgr, channel);
    }

    /** @brief Register a render output targeting @p window. */
    void render(Buffers::BufferManager& mgr, std::shared_ptr<Core::Window> window)
    {
        add_render_sink(m_render_sinks, mgr, std::move(window));
    }

    /** @brief Register a render output targeting @p window with a producer function. */
    void render(Buffers::BufferManager& mgr, std::shared_ptr<Core::Window> window,
        std::function<Kakshya::DataVariant(const InfluenceContext&)> fn)
    {
        add_render_sink(m_render_sinks, mgr, std::move(window), std::move(fn));
    }

    /** @brief Unregister the render sink targeting @p window. */
    void remove_render(Buffers::BufferManager& mgr, const std::shared_ptr<Core::Window>& window)
    {
        remove_render_sink(m_render_sinks, mgr, window);
    }

    /** @brief Push @p samples to all registered audio sinks. */
    void set_audio_data(std::span<const double> samples)
    {
        push_audio_data(m_audio_sinks, samples);
    }

    /** @brief Push @p data as geometry to all registered render sinks. */
    void set_geometry(const Kakshya::DataVariant& data)
    {
        push_geometry(m_render_sinks, data);
    }

    /**
     * @brief Invoke the influence function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    void invoke(const InfluenceContext& ctx) const
    {
        if (m_fn) {
            m_fn(ctx);
        }
        dispatch_audio_sinks(m_audio_sinks, ctx);
        dispatch_render_sinks(m_render_sinks, ctx);
    }

private:
    std::optional<glm::vec3> m_position;
    InfluenceFn m_fn;
    uint32_t m_id {};

    mutable std::vector<AudioSink> m_audio_sinks;
    mutable std::vector<RenderSink> m_render_sinks;

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
