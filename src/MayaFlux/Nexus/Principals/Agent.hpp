#pragma once

#include "MayaFlux/Nexus/Controls.hpp"
#include "MayaFlux/Nexus/Pheme/Attachment.hpp"
#include "MayaFlux/Nexus/Pheme/Influence.hpp"
#include "MayaFlux/Nexus/Pheme/Perception.hpp"

#include "MayaFlux/Nexus/Pheme/Sinks.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Nexus {

/**
 * @class Agent
 * @brief Object that both perceives nearby entities and acts on MayaFlux objects.
 *
 * Constructed with a query radius and, optionally, a perception function and an
 * influence function. Position is optional: set one with `set({ .position = ... })`
 * before registering with @c Fabric if spatial behaviour is required. Without a
 * position the spatial index is not consulted and @c spatial_results will be empty
 * on each commit.
 *
 * Its state is a struct of Quantity members, AgentControls, applied with `set()`
 * from any thread. A member given `Quantity::unset()` is emptied. `extend<C>()`
 * adds a struct of the user's own beside it, as Presence and Locus do for theirs.
 *
 * On each commit the perception function is invoked first, then the influence
 * function. Both receive contexts populated from the same spatial snapshot.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's lifetime.
 *
 * RenderConfig and AttachConfig take designated initializers.
 *
 * @code
 * agent->attach(buf, { .on_move = AttachConfig::OnMove::Carry });
 * agent->render(mgr, { .target_window = window });
 * @endcode
 */
class MAYAFLUX_API Agent {
public:
    using Controls = AgentControls;
    using InfluenceFn = std::function<void(const InfluenceContext&)>;
    using PerceptionFn = std::function<void(const PerceptionContext&)>;

    /**
     * @brief Construct with only a query radius, for an Agent whose behaviour comes
     *        from typed influences and perceptions.
     * @param query_radius Radius passed to the spatial index on each commit.
     *                     Ignored if no position has been set.
     */
    explicit Agent(float query_radius = 1.0F)
    {
        m_controls.query_radius.observe(query_radius);
    }

    /**
     * @brief Construct with query radius, perception function, and influence function.
     * @param query_radius Radius passed to the spatial index on each commit.
     *                     Ignored if no position has been set.
     * @param perception   Called first on every commit.
     * @param influence    Called second on every commit.
     */
    explicit Agent(float query_radius, PerceptionFn perception, InfluenceFn influence)
        : m_perception_fn(std::move(perception))
        , m_influence_fn(std::move(influence))
    {
        m_controls.query_radius.observe(query_radius);
    }

    /**
     * @brief Construct with named perception and influence functions.
     * @param query_radius        Radius passed to the spatial index on each commit.
     * @param perception_fn_name  Identifier for the perception function.
     * @param perception          Called first on every commit.
     * @param influence_fn_name   Identifier for the influence function.
     * @param influence           Called second on every commit.
     */
    Agent(float query_radius,
        std::string perception_fn_name, PerceptionFn perception,
        std::string influence_fn_name, InfluenceFn influence)
        : m_perception_fn_name(std::move(perception_fn_name))
        , m_perception_fn(std::move(perception))
        , m_influence_fn_name(std::move(influence_fn_name))
        , m_influence_fn(std::move(influence))
    {
        m_controls.query_radius.observe(query_radius);
    }

    /** @brief Releases the attached buffer's placement. Sinks are released by the owning Fabric. */
    virtual ~Agent() { detach(); }

    /** @brief Identifier assigned to the perception function, empty if anonymous. */
    [[nodiscard]] const std::string& perception_fn_name() const { return m_perception_fn_name; }

    /** @brief Identifier assigned to the influence function, empty if anonymous. */
    [[nodiscard]] const std::string& influence_fn_name() const { return m_influence_fn_name; }

    /** @brief Set or replace the perception function's identifier. */
    void set_perception_fn_name(std::string name) { m_perception_fn_name = std::move(name); }

    /** @brief Set or replace the influence function's identifier. */
    void set_influence_fn_name(std::string name) { m_influence_fn_name = std::move(name); }

    /** @brief The perception function itself. */
    [[nodiscard]] const PerceptionFn& perception_fn() const { return m_perception_fn; }

    /** @brief The influence function itself. */
    [[nodiscard]] const InfluenceFn& influence_fn() const { return m_influence_fn; }

    // =========================================================================
    // Controls
    // =========================================================================

    /**
     * @brief Apply the members of @p incoming that hold a value.
     * @tparam C AgentControls, or the struct of a derived class or one added with extend().
     *
     * Members left empty are untouched, and `Quantity::unset()` empties a member.
     * Safe from any thread.
     */
    template <typename C = AgentControls>
    void set(const C& incoming)
    {
        m_store.merge(incoming);
    }

    /** @brief A copy of the controls of type C as they are now. */
    template <typename C = AgentControls>
    [[nodiscard]] C get() const
    {
        return m_store.snapshot<C>();
    }

    /** @brief Add a struct of the user's own controls beside AgentControls and return it. */
    template <typename C>
    [[nodiscard]] C& extend()
    {
        return m_store.extend<C>();
    }

    /** @brief The live controls, read without a copy. */
    [[nodiscard]] const AgentControls& controls() const noexcept { return m_controls; }

    /** @brief Every control by name, for tools that list or set them. */
    [[nodiscard]] const QuantityStore& store() const noexcept { return m_store; }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] std::optional<glm::vec3> position() const
    {
        return quantity_value<glm::vec3>(m_controls.position);
    }

    /**
     * @brief Set the position, enabling spatial indexing and queries for this object.
     * @param p World-space coordinates.
     */
    [[deprecated("use set({ .position = ... })")]] void set_position(const glm::vec3& p)
    {
        m_controls.position.assign(Quantity(p));
    }

    /**
     * @brief Clear the position, removing this object from spatial operations.
     */
    [[deprecated("use set({ .position = Quantity::unset() })")]] void clear_position()
    {
        m_controls.position.assign(Quantity::unset());
    }

    /** @brief Set the orientation. Local +Z is forward. Unset means no rotation. */
    [[deprecated("use set({ .orientation = ... })")]] void set_orientation(const glm::quat& q)
    {
        m_controls.orientation.assign(Quantity(q));
    }

    /** @brief Clear the orientation. */
    [[deprecated("use set({ .orientation = Quantity::unset() })")]] void clear_orientation()
    {
        m_controls.orientation.assign(Quantity::unset());
    }

    /** @brief Get the current orientation, if set. */
    [[nodiscard]] std::optional<glm::quat> orientation() const
    {
        return quantity_value<glm::quat>(m_controls.orientation);
    }

    /**
     * @brief Return the query radius.
     */
    [[nodiscard]] float query_radius() const
    {
        return quantity_value<float>(m_controls.query_radius).value_or(1.0F);
    }

    /**
     * @brief Set the query radius.
     * @param r New radius in world-space units.
     */
    [[deprecated("use set({ .query_radius = ... })")]] void set_query_radius(float r)
    {
        m_controls.query_radius.assign(Quantity(r));
    }

    /**
     * @brief Return the stable object id assigned by Fabric.
     */
    [[nodiscard]] uint32_t id() const { return m_id; }

    // =========================================================================
    // Output sinks
    // =========================================================================

    /**
     * @brief Register an audio output on @p channel.
     *
     * Released by the owning Fabric when this entity is removed. Outside a
     * Fabric, call remove_audio_sink().
     */
    void sink_audio(Buffers::BufferManager& mgr, uint32_t channel)
    {
        add_audio_sink(m_audio_sinks, mgr, channel);
    }

    /** @brief Register an audio output on @p channel with a producer function. */
    void sink_audio(Buffers::BufferManager& mgr, uint32_t channel,
        std::function<Kakshya::DataVariant(const InfluenceContext&)> fn,
        std::string fn_name = "")
    {
        add_audio_sink(m_audio_sinks, mgr, channel, std::move(fn), std::move(fn_name));
    }

    /** @brief Unregister the audio sink on @p channel. */
    void remove_audio_sink(Buffers::BufferManager& mgr, uint32_t channel)
    {
        Nexus::remove_audio_sink(m_audio_sinks, mgr, channel);
    }

    /**
     * @brief Register a render output targeting the window in @p config.
     *
     * Released by the owning Fabric when this entity is removed. Outside a
     * Fabric, call remove_render().
     */
    void render(Buffers::BufferManager& mgr, const Portal::Graphics::RenderConfig& config)
    {
        add_render_sink(m_render_sinks, mgr, config, {}, "", position());
    }

    /** @brief Register a render output targeting @p window with a producer function. */
    void render(Buffers::BufferManager& mgr, const Portal::Graphics::RenderConfig& config,
        std::string fn_name, RenderFn fn)
    {
        add_render_sink(m_render_sinks, mgr, config, std::move(fn), std::move(fn_name), position());
    }

    /** @brief Unregister every audio and render sink. */
    void release_sinks(Buffers::BufferManager& mgr)
    {
        release_audio_sinks(m_audio_sinks, mgr);
        release_render_sinks(m_render_sinks, mgr);
    }

    [[nodiscard]] const std::vector<AudioSink>& audio_sinks() const { return m_audio_sinks; }
    [[nodiscard]] const std::vector<RenderSink>& render_sinks() const { return m_render_sinks; }

    /**
     * @brief Make an existing, already rendered buffer the look of this object.
     *
     * The buffer is neither registered nor written to. Its geometry is read
     * once and the object takes its position from the centroid, or from the
     * vertex at config.index, plus config.offset. Every render processor the
     * buffer has is placed at the object's position and orientation. If the
     * object is moved from outside, config.on_move decides whether the move
     * becomes the new offset or carries the buffer. If the object already has
     * a position, that position is kept and becomes the offset. Replaces any
     * previous attachment. The buffer is a live object, so an attachment is not
     * part of encoded state and must be made again after decoding.
     *
     * @param buf    Buffer to attach. Ignored if null.
     * @param config Anchor vertex, offset and move behaviour.
     */
    void attach(const std::shared_ptr<Buffers::VKBuffer>& buf, const AttachConfig& config = {});

    /** @brief Release the attached buffer and reset its placement. */
    void detach();

    /**
     * @brief Read the attached buffer's geometry again and move the anchor to match.
     *
     * The object keeps its position. Blocks for a ComputeMeshBuffer, so call it
     * off the graphics thread.
     */
    void recenter();

    /** @brief True while a buffer is attached. */
    [[nodiscard]] bool attached() const { return m_attachment.has_value(); }

    /**
     * @brief Add an influence of any type, with as many targets as are wanted.
     * @tparam T Type the producer returns; whatever the targets take.
     * @param producer Builds the value from the influence context.
     * @return The influence, to add its targets with Influence::add_target.
     *
     * Runs on every influence after the influence function. Typed influences
     * are runtime objects and are not part of encoded state.
     */
    template <typename T>
    std::shared_ptr<Influence<T>> add_influence(typename Influence<T>::Producer producer)
    {
        auto influence = std::make_shared<Influence<T>>(std::move(producer));
        m_influences.emplace_back(influence,
            [influence](const InfluenceContext& ctx) { influence->invoke(ctx); });
        return influence;
    }

    /** @brief Stop an influence added with add_influence(). */
    template <typename T>
    void remove_influence(const std::shared_ptr<Influence<T>>& influence)
    {
        std::erase_if(m_influences,
            [&influence](const auto& entry) { return entry.first == influence; });
    }

    /** @brief Stop every influence added with add_influence(). */
    void clear_influences() { m_influences.clear(); }

    /**
     * @brief Add a perception of any type, with as many targets as are wanted.
     * @tparam T Type the source returns; whatever the targets take.
     * @param source Reads the value, using the perception context.
     * @return The perception, to add its targets with Perception::add_target.
     *
     * Runs on every perception before the perception function, so the function
     * can use what the targets received. Typed perceptions are runtime objects
     * and are not part of encoded state.
     */
    template <typename T>
    std::shared_ptr<Perception<T>> add_perception(typename Perception<T>::Source source)
    {
        auto perception = std::make_shared<Perception<T>>(std::move(source));
        m_perceptions.emplace_back(perception,
            [perception](const PerceptionContext& ctx) { perception->invoke(ctx); });
        return perception;
    }

    /** @brief Stop a perception added with add_perception(). */
    template <typename T>
    void remove_perception(const std::shared_ptr<Perception<T>>& perception)
    {
        std::erase_if(m_perceptions,
            [&perception](const auto& entry) { return entry.first == perception; });
    }

    /** @brief Stop every perception added with add_perception(). */
    void clear_perceptions() { m_perceptions.clear(); }

    /* @brief Return the render processor for the sink targeting @p window, or nullptr if not found. */
    std::shared_ptr<Buffers::RenderProcessor> get_render_processor(
        const std::shared_ptr<Core::Window>& window) const
    {
        auto it = std::ranges::find_if(m_render_sinks,
            [&window](const RenderSink& s) { return s.window == window; });
        return it != m_render_sinks.end() ? it->renderer : nullptr;
    }

    /* @brief Return the render processor for the sink drawing into @p image, or nullptr if not found. */
    std::shared_ptr<Buffers::RenderProcessor> get_render_processor(
        const std::shared_ptr<Portal::Graphics::ImageTarget>& image) const
    {
        auto it = std::ranges::find_if(m_render_sinks, [&image](const RenderSink& s) {
            return std::ranges::find(s.images, image) != s.images.end();
        });
        return it != m_render_sinks.end() ? it->renderer : nullptr;
    }

    /** @brief Unregister the render sink targeting @p window. */
    void remove_render(Buffers::BufferManager& mgr, const std::shared_ptr<Core::Window>& window)
    {
        remove_render_sink(m_render_sinks, mgr, window);
    }

    /** @brief Unregister the render sink drawing into @p image. */
    void remove_render(Buffers::BufferManager& mgr, const std::shared_ptr<Portal::Graphics::ImageTarget>& image)
    {
        remove_render_sink(m_render_sinks, mgr, image);
    }

    /** @brief Push @p samples to all registered audio sinks. */
    void set_audio_data(std::span<const double> samples)
    {
        push_audio_data(m_audio_sinks, samples);
    }

    /**
     * @brief Set pre-packed interleaved vertex bytes to all registered render sinks.
     *
     * data must point to N contiguous 68-byte Vertex records.
     * byte_count must be a multiple of 68.
     */
    void set_vertices(const void* data, size_t byte_count);

    /**
     * @brief Set typed vertex data to all registered render sinks.
     * @tparam T One of Kakshya::PointVertex, Kakshya::LineVertex, Kakshya::MeshVertex, Kakshya::Vertex.
     */
    template <typename T>
    void set_vertices(std::span<const T> vertices)
    {
        static_assert(sizeof(T) == 68, "set_vertices: T must be a 68-byte vertex type");
        set_vertices(vertices.data(), vertices.size_bytes());
    }

    /** @brief Set the intensity, a general-purpose parameter for influence functions. */
    [[deprecated("use set({ .intensity = ... })")]] void set_intensity(float i)
    {
        m_controls.intensity.assign(Quantity(i));
    }

    /** @brief Get the current intensity. */
    [[nodiscard]] float intensity() const
    {
        return quantity_value<float>(m_controls.intensity).value_or(1.0F);
    }

    /** @brief Set the radius, a general-purpose parameter for influence functions. */
    [[deprecated("use set({ .radius = ... })")]] void set_radius(float r)
    {
        m_controls.radius.assign(Quantity(r));
    }

    /** @brief Get the current radius. */
    [[nodiscard]] float radius() const
    {
        return quantity_value<float>(m_controls.radius).value_or(1.0F);
    }

    /** @brief Set the color, a general-purpose parameter for influence functions. */
    [[deprecated("use set({ .color = ... })")]] void set_color(const glm::vec3& c)
    {
        m_controls.color.assign(Quantity(c));
    }

    /** @brief Clear the color, resetting it to an unset state. */
    [[deprecated("use set({ .color = Quantity::unset() })")]] void clear_color()
    {
        m_controls.color.assign(Quantity::unset());
    }

    /** @brief Get the current color, if set. */
    [[nodiscard]] std::optional<glm::vec3> color() const
    {
        return quantity_value<glm::vec3>(m_controls.color);
    }

    /** @brief Set the size, a general-purpose parameter for influence functions. */
    [[deprecated("use set({ .size = ... })")]] void set_size(float s)
    {
        m_controls.size.assign(Quantity(s));
    }

    /** @brief Clear the size, resetting it to an unset state. */
    [[deprecated("use set({ .size = Quantity::unset() })")]] void clear_size()
    {
        m_controls.size.assign(Quantity::unset());
    }

    /** @brief Get the current size, if set. */
    [[nodiscard]] std::optional<float> size() const
    {
        return quantity_value<float>(m_controls.size);
    }

    /**
     * @brief Invoke the perception function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    virtual void invoke_perception(const PerceptionContext& ctx)
    {
        for (const auto& [handle, perception] : m_perceptions) {
            perception(ctx);
        }
        if (m_perception_fn) {
            m_perception_fn(ctx);
        }
    }

    /**
     * @brief Invoke the influence function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    virtual void invoke_influence(const InfluenceContext& ctx) const
    {
        if (m_influence_fn) {
            m_influence_fn(ctx);
        }
        dispatch_audio_sinks(m_audio_sinks, ctx);
        dispatch_render_sinks(m_render_sinks, ctx);
        if (m_attachment)
            apply_attachment(*m_attachment, ctx);
        for (const auto& [handle, influence] : m_influences) {
            influence(ctx);
        }
    }

private:
    [[nodiscard]] static AgentControls& open(QuantityStore& store);

    QuantityStore m_store;

protected:
    AgentControls& m_controls { open(m_store) };

private:
    std::string m_perception_fn_name;
    PerceptionFn m_perception_fn;
    std::string m_influence_fn_name;
    InfluenceFn m_influence_fn;
    uint32_t m_id {};

    mutable std::vector<AudioSink> m_audio_sinks;
    mutable std::vector<RenderSink> m_render_sinks;
    mutable std::optional<Attachment> m_attachment;
    std::vector<std::pair<std::shared_ptr<void>, InfluenceFn>> m_influences;
    std::vector<std::pair<std::shared_ptr<void>, PerceptionFn>> m_perceptions;

    void follow_attachment();

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
