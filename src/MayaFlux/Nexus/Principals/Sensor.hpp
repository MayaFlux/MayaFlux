#pragma once

#include "MayaFlux/Nexus/Controls.hpp"
#include "MayaFlux/Nexus/Pheme/Perception.hpp"
#include "MayaFlux/Transitive/Reflect/FunctionCall.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Sensor
 * @brief Object that reacts to nearby entities when committed.
 *
 * Constructed with a query radius and, optionally, a perception function. Position
 * is optional: set one with `set({ .position = ... })` before registering with
 * @c Fabric if spatial queries are required. A Sensor without a position receives an
 * empty @c spatial_results span on each commit.
 *
 * Its state is structs of Quantity members, PlacementControls and SensorControls,
 * applied with `set()` from any thread. `extend<C>()` adds a struct of the user's
 * own beside them.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's
 * lifetime.
 */
class MAYAFLUX_API Sensor {
public:
    using Controls = SensorControls;
    using PerceptionFn = std::function<void(const PerceptionContext&)>;

    /**
     * @brief Construct with only a query radius, for a Sensor whose behaviour comes
     *        from a typed perception.
     * @param query_radius Radius passed to the spatial index on each commit.
     *                     Ignored if no position has been set.
     */
    explicit Sensor(float query_radius = 1.0F)
    {
        m_controls.query_radius.observe(query_radius);
    }

    /**
     * @brief Construct with a query radius and a perception function.
     * @param query_radius Radius passed to the spatial index on each commit.
     *                     Ignored if no position has been set.
     * @param fn           Called on every commit with the current context.
     */
    explicit Sensor(float query_radius, PerceptionFn fn)
        : m_fn(std::move(fn))
    {
        m_controls.query_radius.observe(query_radius);
    }

    /**
     * @brief Construct with a query radius and a named perception function.
     * @param query_radius Radius passed to the spatial index on each commit.
     * @param fn_name      Identifier used for state encoding.
     * @param fn           Called on every commit with the current context.
     */
    Sensor(float query_radius, std::string fn_name, PerceptionFn fn)
        : m_fn_call(std::move(fn_name), Reflect::FunctionCall::second_type {})
        , m_fn(std::move(fn))
    {
        m_controls.query_radius.observe(query_radius);
    }

    /**
     * @brief Apply the members of @p incoming that hold a value.
     * @tparam C PlacementControls, SensorControls, or a struct added with extend().
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

    /** @brief The live SensorControls, read without a copy. */
    [[nodiscard]] const SensorControls& controls() const noexcept { return m_controls; }

    /** @brief Every control by name, for tools that list or set them. */
    [[nodiscard]] const QuantityStore& store() const noexcept { return m_store; }

    /** @brief Identifier assigned to the perception function, empty if anonymous. */
    [[nodiscard]] const std::string& fn_name() const { return m_fn_call.first; }

    /** @brief The perception function as a name and its arguments. */
    [[nodiscard]] const Reflect::FunctionCall& fn_call() const { return m_fn_call; }

    /** @brief Set or replace the perception function's identifier. */
    void set_fn_name(std::string name) { m_fn_call = { std::move(name), {} }; }

    /** @brief Say how the perception function was made, so it can be made again. */
    void set_fn_call(Reflect::FunctionCall call) { m_fn_call = std::move(call); }

    /** @brief The perception function itself. */
    [[nodiscard]] const PerceptionFn& fn() const { return m_fn; }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] std::optional<glm::vec3> position() const
    {
        return quantity_value<glm::vec3>(m_placement.position);
    }

    /**
     * @brief Set the position, enabling spatial queries for this object.
     * @param p World-space coordinates.
     */
    [[deprecated("use set({ .position = ... })")]] void set_position(const glm::vec3& p)
    {
        m_placement.position.assign(Quantity(p));
    }

    /**
     * @brief Clear the position, disabling spatial queries for this object.
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
    [[deprecated("use set(SensorControls { .query_radius = ... })")]] void set_query_radius(float r)
    {
        m_controls.query_radius.assign(Quantity(r));
    }

    /**
     * @brief Return the stable object id assigned by Fabric.
     */
    [[nodiscard]] uint32_t id() const { return m_id; }

    /**
     * @brief Set the one source of a perception of any type.
     * @tparam T Type the source returns; whatever the target takes.
     * @param source Reads the value, using the perception context.
     * @param target Takes the value and decides where it lands.
     *
     * A Sensor has a single source: setting another replaces it. Runs on every
     * invoke before the perception function. Not part of encoded state.
     */
    template <typename T>
    void set_perception(typename Perception<T>::Source source, typename Perception<T>::Target target)
    {
        auto perception = std::make_shared<Perception<T>>(std::move(source));
        perception->add_target(std::move(target));
        m_perception = [perception](const PerceptionContext& ctx) { perception->invoke(ctx); };
    }

    /** @brief Remove the source set with set_perception(). */
    void clear_perception() { m_perception = nullptr; }

    /**
     * @brief Invoke the perception function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    void invoke(const PerceptionContext& ctx) const
    {
        if (m_perception) {
            m_perception(ctx);
        }
        if (m_fn) {
            m_fn(ctx);
        }
    }

private:
    [[nodiscard]] static PlacementControls& open_placement(QuantityStore& store);
    [[nodiscard]] static SensorControls& open(QuantityStore& store);

    QuantityStore m_store;
    PlacementControls& m_placement { open_placement(m_store) };
    SensorControls& m_controls { open(m_store) };
    Reflect::FunctionCall m_fn_call;
    PerceptionFn m_fn;
    PerceptionFn m_perception;
    uint32_t m_id { 0 };

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
