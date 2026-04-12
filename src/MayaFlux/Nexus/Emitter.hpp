#pragma once

#include "InfluenceContext.hpp"
#include "Sinks.hpp"

#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"

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
    void render(Buffers::BufferManager& mgr, const Portal::Graphics::RenderConfig& config)
    {
        add_render_sink(m_render_sinks, mgr, config, {}, m_position);
    }

    /** @brief Register a render output targeting @p window with a producer function. */
    void render(Buffers::BufferManager& mgr, const Portal::Graphics::RenderConfig& config, RenderFn fn)
    {
        add_render_sink(m_render_sinks, mgr, config, std::move(fn), m_position);
    }

    /* @brief Return the render processor for the sink targeting @p window, or nullptr if not found. */
    std::shared_ptr<Buffers::RenderProcessor> get_render_processor(
        const std::shared_ptr<Core::Window>& window) const
    {
        auto it = std::ranges::find_if(m_render_sinks,
            [&window](const RenderSink& s) { return s.window == window; });
        return it != m_render_sinks.end() ? it->renderer : nullptr;
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

    /**
     * @brief Push pre-resolved vertex bytes to all registered render sinks.
     * @param data       Pointer to vertex data.
     * @param byte_count Total size in bytes.
     * @param layout     VertexLayout describing stride and attributes.
     */
    void set_vertices(const void* data, size_t byte_count,
        const Kakshya::VertexLayout& layout)
    {
        push_vertices(m_render_sinks, data, byte_count, layout);
    }

    /**
     * @brief Push typed vertex data to all registered render sinks.
     * @tparam T One of Nodes::PointVertex, Nodes::LineVertex, Nodes::MeshVertex.
     * @param vertices Span of vertex structs.
     */
    template <typename T>
    void set_vertices(std::span<const T> vertices)
    {
        auto layout = Nodes::vertex_layout_for<T>();
        layout.vertex_count = static_cast<uint32_t>(vertices.size());
        push_vertices(m_render_sinks, vertices.data(),
            vertices.size_bytes(), layout);
    }

    /** @brief Set the intensity, a general-purpose parameter for influence functions. */
    void set_intensity(float i) { m_intensity = i; }

    /** @brief Get the current intensity. */
    [[nodiscard]] float intensity() const { return m_intensity; }

    /** @brief Set the radius, a general-purpose parameter for influence functions. */
    void set_radius(float r) { m_radius = r; }

    /** @brief Get the current radius. */
    [[nodiscard]] float radius() const { return m_radius; }

    /** @brief Set the color, a general-purpose parameter for influence functions. */
    void set_color(const glm::vec3& c) { m_color = c; }

    /** @brief Clear the color, resetting it to an unset state. */
    void clear_color() { m_color.reset(); }

    /** @brief Get the current color, if set. */
    [[nodiscard]] const std::optional<glm::vec3>& color() const { return m_color; }

    /** @brief Set the size, a general-purpose parameter for influence functions. */
    void set_size(float s) { m_size = s; }

    /** @brief Clear the size, resetting it to an unset state. */
    void clear_size() { m_size.reset(); }

    /** @brief Get the current size, if set. */
    [[nodiscard]] const std::optional<float>& size() const { return m_size; }

    /* @brief Set the render processor to target for this influence, if applicable. */
    void set_influence_target(std::shared_ptr<Buffers::RenderProcessor> proc) { m_influence_target = std::move(proc); }

    /* @brief Clear the influence target, resetting it to an unset state. */
    void clear_influence_target() { m_influence_target.reset(); }

    /* @brief Get the current influence target, if set. */
    [[nodiscard]] std::weak_ptr<Buffers::RenderProcessor> influence_target() const { return m_influence_target; }

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
    std::optional<glm::vec3> m_color;
    std::optional<float> m_size;
    float m_intensity { 1.0F };
    float m_radius { 1.0F };

    std::shared_ptr<Buffers::RenderProcessor> m_influence_target;

    InfluenceFn m_fn;
    uint32_t m_id {};

    mutable std::vector<AudioSink> m_audio_sinks;
    mutable std::vector<RenderSink> m_render_sinks;

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
