#pragma once

#include "InfluenceContext.hpp"
#include "PerceptionContext.hpp"

#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"

#include "Sinks.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Agent
 * @brief Object that both perceives nearby entities and acts on MayaFlux objects.
 *
 * Constructed with only a query radius, a perception function, and an influence
 * function. Position is optional: call @c set_position before registering with
 * @c Fabric if spatial behaviour is required. Without a position the spatial
 * index is not consulted and @c spatial_results will be empty on each commit.
 *
 * On each commit the perception function is invoked first, then the influence
 * function. Both receive contexts populated from the same spatial snapshot.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's lifetime.
 */
class Agent {
public:
    using InfluenceFn = std::function<void(const InfluenceContext&)>;
    using PerceptionFn = std::function<void(const PerceptionContext&)>;

    /**
     * @brief Construct with query radius, perception function, and influence function.
     * @param query_radius Radius passed to the spatial index on each commit.
     *                     Ignored if no position has been set.
     * @param perception   Called first on every commit.
     * @param influence    Called second on every commit.
     */
    explicit Agent(float query_radius, PerceptionFn perception, InfluenceFn influence)
        : m_query_radius(query_radius)
        , m_perception_fn(std::move(perception))
        , m_influence_fn(std::move(influence))
    {
    }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] const std::optional<glm::vec3>& position() const { return m_position; }

    /**
     * @brief Set the position, enabling spatial indexing and queries for this object.
     * @param p World-space coordinates.
     */
    void set_position(const glm::vec3& p) { m_position = p; }

    /**
     * @brief Clear the position, removing this object from spatial operations.
     */
    void clear_position() { m_position.reset(); }

    /**
     * @brief Return the query radius.
     */
    [[nodiscard]] float query_radius() const { return m_query_radius; }

    /**
     * @brief Set the query radius.
     * @param r New radius in world-space units.
     */
    void set_query_radius(float r) { m_query_radius = r; }

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

    /**
     * @brief Invoke the perception function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    void invoke_perception(const PerceptionContext& ctx) const
    {
        if (m_perception_fn) {
            m_perception_fn(ctx);
        }
    }

    /**
     * @brief Invoke the influence function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    void invoke_influence(const InfluenceContext& ctx) const
    {
        if (m_influence_fn) {
            m_influence_fn(ctx);
        }
        dispatch_audio_sinks(m_audio_sinks, ctx);
        dispatch_render_sinks(m_render_sinks, ctx);
    }

private:
    std::optional<glm::vec3> m_position;
    float m_query_radius;
    PerceptionFn m_perception_fn;
    InfluenceFn m_influence_fn;
    uint32_t m_id {};

    mutable std::vector<AudioSink> m_audio_sinks;
    mutable std::vector<RenderSink> m_render_sinks;

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
