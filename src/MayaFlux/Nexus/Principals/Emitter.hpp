#pragma once

#include "MayaFlux/Nexus/Controls.hpp"
#include "MayaFlux/Nexus/Pheme/Attachment.hpp"
#include "MayaFlux/Nexus/Pheme/Influence.hpp"
#include "MayaFlux/Nexus/Pheme/Sinks.hpp"
#include "MayaFlux/Transitive/Reflect/FunctionCall.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Nexus {

class StateEncoder;

/**
 * @class Emitter
 * @brief Object that acts on existing MayaFlux objects when committed.
 *
 * Constructed with an optional influence function. Position is optional: set one
 * with `set({ .position = ... })` before registering with @c Fabric if spatial
 * indexing is required. Entities without a position are committed normally but are not
 * inserted into the spatial index.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's
 * lifetime. It is zero until the object has been registered.
 *
 * RenderConfig takes a designated initializer.
 *
 * Its state is structs of Quantity members: PlacementControls for where it stands
 * and EmitterControls for what it puts out. `set()` applies the members that hold
 * a value, from any thread, and a member given `Quantity::unset()` is emptied.
 * The getters read the state as it is now. `extend<C>()` adds a struct of the
 * user's own beside them.
 *
 * @code
 * emitter->set({ .position = glm::vec3(0, 1, 0) });
 * emitter->set(EmitterControls { .intensity = 0.5 });
 * emitter->set({ .position = Quantity::unset() });
 * emitter->set_influence<InfluenceBlock>(InfluenceBlock::from, bind_influence_block(proc));
 * emitter->render(mgr, { .target_window = window });
 * @endcode
 */
class MAYAFLUX_API Emitter {
public:
    using Controls = EmitterControls;
    using InfluenceFn = std::function<void(const InfluenceContext&)>;

    /**
     * @brief Construct with no function, for an Emitter whose behaviour comes
     *        from a typed influence.
     */
    Emitter() = default;

    /** @brief Releases the attached buffer's placement. Sinks are released by the owning Fabric. */
    ~Emitter() { detach(); }

    /**
     * @brief Construct with an influence function.
     * @param fn Called on every commit with the current context.
     */
    explicit Emitter(InfluenceFn fn)
        : m_fn(std::move(fn))
    {
    }

    /**
     * @brief Construct with a named influence function.
     * @param fn_name Identifier used for state encoding.
     * @param fn      Called on every commit with the current context.
     */
    Emitter(std::string fn_name, InfluenceFn fn)
        : m_fn_call(std::move(fn_name), Reflect::FunctionCall::second_type {})
        , m_fn(std::move(fn))
    {
    }

    /** @brief Identifier assigned to the influence function, empty if anonymous. */
    [[nodiscard]] const std::string& fn_name() const { return m_fn_call.first; }

    /** @brief The influence function as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& fn_call() const { return m_fn_call; }

    /** @brief Set or replace the influence function's identifier. */
    void set_fn_name(std::string name) { m_fn_call = { std::move(name), {} }; }

    /** @brief Say how the influence function was made, so it can be made again. */
    void set_fn_call(Reflect::FunctionCall call) { m_fn_call = std::move(call); }

    /** @brief The influence function itself. */
    [[nodiscard]] const InfluenceFn& fn() const { return m_fn; }

    // =========================================================================
    // Controls
    // =========================================================================

    /**
     * @brief Apply the members of @p incoming that hold a value.
     * @tparam C PlacementControls, EmitterControls, or a struct added with extend().
     *
     * Members left empty are untouched, and `Quantity::unset()` empties a member.
     * Safe from any thread.
     */
    template <typename C = PlacementControls>
    void set(const C& incoming)
    {
        m_store.merge(incoming);
    }

    /** @brief A copy of the controls of type C as they are now. */
    template <typename C = PlacementControls>
    [[nodiscard]] C get() const
    {
        return m_store.snapshot<C>();
    }

    /** @brief Add a struct of the user's own controls beside the built-in ones and return it. */
    template <typename C>
    [[nodiscard]] C& extend()
    {
        return m_store.extend<C>();
    }

    /** @brief The live EmitterControls, read without a copy. */
    [[nodiscard]] const EmitterControls& controls() const noexcept { return m_controls; }

    /** @brief Every control by name, for tools that list or set them. */
    [[nodiscard]] const QuantityStore& store() const noexcept { return m_store; }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] std::optional<glm::vec3> position() const
    {
        return quantity_value<glm::vec3>(m_placement.position);
    }

    /**
     * @brief Set the position, enabling spatial indexing for this object.
     * @param p World-space coordinates.
     */
    [[deprecated("use set({ .position = ... })")]] void set_position(const glm::vec3& p)
    {
        m_placement.position.assign(Quantity(p));
    }

    /**
     * @brief Clear the position, removing this object from spatial queries.
     */
    [[deprecated("use set({ .position = Quantity::unset() })")]] void clear_position()
    {
        m_placement.position.assign(Quantity::unset());
    }

    /** @brief Return the current orientation, if set. Local -Z is forward. */
    [[nodiscard]] std::optional<glm::quat> orientation() const
    {
        return quantity_value<glm::quat>(m_placement.orientation);
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

    /** @brief sink_audio() with the call that made @p fn, which a saved state makes it from again. */
    void sink_audio(Buffers::BufferManager& mgr, uint32_t channel, const Reflect::FunctionCall& call,
        std::function<Kakshya::DataVariant(const InfluenceContext&)> fn)
    {
        add_audio_sink(m_audio_sinks, mgr, channel, std::move(fn), call.first);
        m_audio_sinks.back().arguments = call.second;
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

    /** @brief render() with the call that made @p fn, which a saved state makes it from again. */
    void render(Buffers::BufferManager& mgr, const Portal::Graphics::RenderConfig& config,
        const Reflect::FunctionCall& call, RenderFn fn)
    {
        add_render_sink(m_render_sinks, mgr, config, std::move(fn), call.first, position());
        m_render_sinks.back().arguments = call.second;
    }

    /** @brief Unregister every audio and render sink. */
    void release_sinks(Buffers::BufferManager& mgr)
    {
        release_audio_sinks(m_audio_sinks, mgr);
        release_render_sinks(m_render_sinks, mgr);
    }

    [[nodiscard]] const std::vector<AudioSink>& audio_sinks() const { return m_audio_sinks; }
    [[nodiscard]] const std::vector<RenderSink>& render_sinks() const { return m_render_sinks; }

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
    [[deprecated("use set(EmitterControls { .intensity = ... })")]] void set_intensity(float i)
    {
        m_controls.intensity.assign(Quantity(i));
    }

    /** @brief Get the current intensity. */
    [[nodiscard]] float intensity() const
    {
        return quantity_value<float>(m_controls.intensity).value_or(1.0F);
    }

    /** @brief Set the radius, a general-purpose parameter for influence functions. */
    [[deprecated("use set(EmitterControls { .radius = ... })")]] void set_radius(float r)
    {
        m_controls.radius.assign(Quantity(r));
    }

    /** @brief Get the current radius. */
    [[nodiscard]] float radius() const
    {
        return quantity_value<float>(m_controls.radius).value_or(1.0F);
    }

    /** @brief Set the color, a general-purpose parameter for influence functions. */
    [[deprecated("use set(EmitterControls { .color = ... })")]] void set_color(const glm::vec3& c)
    {
        m_controls.color.assign(Quantity(c));
    }

    /** @brief Clear the color, resetting it to an unset state. */
    [[deprecated("use set(EmitterControls { .color = Quantity::unset() })")]] void clear_color()
    {
        m_controls.color.assign(Quantity::unset());
    }

    /** @brief Get the current color, if set. */
    [[nodiscard]] std::optional<glm::vec3> color() const
    {
        return quantity_value<glm::vec3>(m_controls.color);
    }

    /** @brief Set the size, a general-purpose parameter for influence functions. */
    [[deprecated("use set(EmitterControls { .size = ... })")]] void set_size(float s)
    {
        m_controls.size.assign(Quantity(s));
    }

    /** @brief Clear the size, resetting it to an unset state. */
    [[deprecated("use set(EmitterControls { .size = Quantity::unset() })")]] void clear_size()
    {
        m_controls.size.assign(Quantity::unset());
    }

    /** @brief Get the current size, if set. */
    [[nodiscard]] std::optional<float> size() const
    {
        return quantity_value<float>(m_controls.size);
    }

    /** @brief The cone, if set: inner and outer angles, radians from the axis. */
    [[nodiscard]] std::optional<glm::vec2> cone() const
    {
        return quantity_value<glm::vec2>(m_controls.cone);
    }

    /** @brief The attenuation, if set: constant, linear and quadratic terms. */
    [[nodiscard]] std::optional<glm::vec3> attenuation() const
    {
        return quantity_value<glm::vec3>(m_controls.attenuation);
    }

    /** @brief The shape, if set. Unset is a point. */
    [[nodiscard]] std::optional<SourceShape> shape() const
    {
        return quantity_value<SourceShape>(m_controls.shape);
    }

    /** @brief The measures of the shape, if set. */
    [[nodiscard]] std::optional<glm::vec2> extent() const
    {
        return quantity_value<glm::vec2>(m_controls.extent);
    }

    /**
     * @brief Make an existing, already rendered buffer the look of this object.
     *
     * Works as Agent::attach does: every render processor the buffer has is
     * placed at the object's position and orientation. If the object already has
     * a position, that position is kept and becomes the offset. Replaces any
     * previous attachment. StateEncoder saves it when the buffer has a bound name.
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
     * @brief Run @p apply on this object and keep its call, so a loaded state can run it again.
     *
     * For what only code can set up, such as a typed influence or perception.
     * Make @p apply from the same call with a Reflect::FunctionTable.
     */
    void configure(Reflect::FunctionCall call, const std::function<void(Emitter&)>& apply)
    {
        const auto before = sink_buffers();
        apply(*this);
        for (const void* sink : sink_buffers()) {
            if (std::ranges::find(before, sink) == before.end()) {
                m_configured_sinks.push_back(sink);
            }
        }
        m_configurations.push_back(std::move(call));
    }

    /** @brief The calls configure() was given, in order. */
    [[nodiscard]] const std::vector<Reflect::FunctionCall>& configurations() const { return m_configurations; }

    /** @brief True for a sink buffer that configure() made, which its call makes again. */
    [[nodiscard]] bool made_by_configure(const void* sink) const
    {
        return std::ranges::find(m_configured_sinks, sink) != m_configured_sinks.end();
    }

    /**
     * @brief Set the one target of an influence of any type.
     * @tparam T Type the producer returns; whatever the target takes.
     * @param producer Builds the value from the influence context.
     * @param target   Takes the value and decides what it affects.
     *
     * An Emitter has a single target: setting another replaces it. Runs on
     * every invoke after the influence function, independent of the render
     * processor target. Not part of encoded state.
     */
    template <typename T>
    void set_influence(typename Influence<T>::Producer producer, typename Influence<T>::Target target)
    {
        auto influence = std::make_shared<Influence<T>>(std::move(producer));
        influence->add_target(std::move(target));
        m_influence = [influence](const InfluenceContext& ctx) { influence->invoke(ctx); };
    }

    /** @brief Remove the target set with set_influence(). */
    void clear_influence() { m_influence = nullptr; }

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
        if (m_attachment)
            apply_attachment(*m_attachment, ctx);
        if (m_influence) {
            m_influence(ctx);
        }
    }

private:
    [[nodiscard]] static PlacementControls& open_placement(QuantityStore& store);
    [[nodiscard]] static EmitterControls& open(QuantityStore& store);

    QuantityStore m_store;
    PlacementControls& m_placement { open_placement(m_store) };
    EmitterControls& m_controls { open(m_store) };

    InfluenceFn m_fn;
    uint32_t m_id {};
    Reflect::FunctionCall m_fn_call;

    mutable std::vector<AudioSink> m_audio_sinks;
    mutable std::vector<RenderSink> m_render_sinks;
    InfluenceFn m_influence;
    [[nodiscard]] std::vector<const void*> sink_buffers() const
    {
        std::vector<const void*> held;
        for (const auto& sink : m_audio_sinks) {
            held.push_back(sink.buf.get());
        }
        for (const auto& sink : m_render_sinks) {
            held.push_back(sink.buf.get());
        }
        return held;
    }

    std::vector<const void*> m_configured_sinks;
    std::vector<Reflect::FunctionCall> m_configurations;
    mutable std::optional<Attachment> m_attachment;

    void follow_attachment();

    friend class Fabric;
    friend class StateEncoder;
    friend class StateDecoder;
};

} // namespace MayaFlux::Nexus
