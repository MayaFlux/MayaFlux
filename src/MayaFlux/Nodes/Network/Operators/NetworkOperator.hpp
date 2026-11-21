#pragma once

#include "MayaFlux/Nodes/Network/Control.hpp"
#include "ParameterResolver.hpp"

namespace MayaFlux::Nodes::Network {

class NodeNetwork;

/**
 * @class NetworkOperator
 * @brief Domain-agnostic interpretive lens for network processing
 *
 * Base interface for all operators (graphics, audio, control).
 * Operators own their data representation and processing logic.
 * No assumptions about position types, vertex formats, or domains.
 */
class MAYAFLUX_API NetworkOperator {
public:
    virtual ~NetworkOperator() = default;

    /**
     * @brief Process for one batch cycle
     * @param dt Time delta or sample count (operator-specific)
     */
    virtual void process(float dt) = 0;

    /**
     * @brief Set operator parameter
     */
    virtual void set_parameter(std::string_view param, double value) = 0;

    /** @brief Apply a cached mapping key, delegating to set_parameter by default. */
    virtual void set_mapped_parameter(ParameterKey key, double value)
    {
        set_parameter(key.name, value);
    }

    /** @brief Apply a cached per-element key, delegating to apply_one_to_one by default. */
    virtual void apply_mapped_one_to_one(
        ParameterKey key, const std::shared_ptr<NodeNetwork>& source)
    {
        apply_one_to_one(key.name, source);
    }

    /**
     * @brief Query operator internal state
     */
    [[nodiscard]] virtual std::optional<double> query_state(std::string_view query) const = 0;

    /**
     * @brief Type name for introspection
     */
    [[nodiscard]] virtual std::string_view get_type_name() const = 0;

    /**
     * @brief Apply ONE_TO_ONE parameter mapping (per-point control)
     * @param param Parameter name (e.g., "force_x", "color", "mass")
     * @param source Source network providing per-point values
     *
     * Default implementation does nothing. Operators that support
     * per-point control override this method.
     */
    virtual void apply_one_to_one(
        std::string_view param,
        const std::shared_ptr<NodeNetwork>& source)
    {
        // Default: no-op (operator doesn't support per-point control)
    }

    /**
     * @brief Whether a downstream compute pass writes the network's vertex
     *        buffer in place each cycle, so the geometry producer must keep
     *        restoring the CPU-authoritative vertex data every frame.
     *
     * False by default. A GpuFieldOperator with bindings returns true: its
     * VertexFieldProcessor adds a field onto the existing records (pos += f),
     * a displacement from the uploaded geometry rather than an integrator,
     * bounded only while the producer re-uploads the reference geometry every
     * frame. NetworkGeometryProcessor suppresses its incremental /
     * skip-when-clean upload path while any chain operator reports this. A
     * later seed/render split gives the field pass its own reference and
     * lifts the restriction.
     */
    [[nodiscard]] virtual bool demands_full_vertex_restore() const { return false; }

    /**
     * @brief Apply the members of this operator's Controls struct that hold a value.
     *
     * An operator opts in with `using Controls = ...`, a struct of Control
     * members declared with MF_DESCRIBE, and holds the reference
     * declare_controls() returns. Members left empty are untouched, so
     * `op->set({ .drag = 0.1 })` changes only drag. A node source maps in
     * BROADCAST and a network source in ONE_TO_ONE. A number may be set from
     * any thread while a block renders. Attaching or removing a node or network
     * source belongs outside process. The string based set_parameter stays for
     * names a user invents.
     */
    template <typename Self>
    void set(this Self& self, const typename Self::Controls& incoming)
    {
        static_cast<NetworkOperator&>(self).m_controls_store.merge(incoming);
    }

    /**
     * @brief The controls with the values the operator holds now.
     *
     * An operator may offer read_controls(Controls&) to fill members whose
     * value lives elsewhere.
     */
    template <typename Self>
    [[nodiscard]] typename Self::Controls get(this const Self& self)
    {
        const NetworkOperator& base = self;
        auto current = base.m_controls_store.snapshot<typename Self::Controls>();
        if constexpr (requires { self.read_controls(current); }) {
            self.read_controls(current);
        }
        return current;
    }

protected:
    /**
     * @brief Create the operator's controls struct and keep it alive.
     * @return The struct, owned by this operator, for the derived class to read.
     *
     * Call once from the constructor's initializer list and hold the result in
     * a reference member. The struct must be described and every member a
     * Control.
     */
    template <typename C>
    [[nodiscard]] C& declare_controls()
    {
        return m_controls_store.declare<C>();
    }

    /**
     * @brief Apply the members of @p incoming that hold a value, for an operator whose controls type is not fixed.
     * @tparam C The struct the operator declared with declare_controls().
     */
    template <typename C>
    void merge_controls(const C& incoming)
    {
        m_controls_store.merge(incoming);
    }

    /**
     * @brief A copy of the declared struct, for an operator whose controls type is not fixed.
     * @tparam C The struct the operator declared with declare_controls().
     */
    template <typename C>
    [[nodiscard]] C snapshot_controls() const
    {
        return m_controls_store.snapshot<C>();
    }

    /**
     * @brief Read node sources into their controls.
     *
     * Call once per block from process() before reading the controls struct.
     */
    void refresh_controls() { m_controls_store.refresh(); }

    /**
     * @brief Sets the control called @p name, the way set() would.
     * @return False when no control has that name.
     *
     * For an operator's string based set_parameter, so a name resolves through
     * its controls and not through a table of its own.
     */
    bool set_named_control(std::string_view name, double value)
    {
        return m_controls_store.set_named(name, value);
    }

    /**
     * @brief Drops the pending typed value of the control called @p name.
     *
     * Call at the top of an immediate string write, so a typed set made
     * earlier does not override it at the next block. A name that is not a
     * control is ignored.
     */
    bool settle_named_control(std::string_view name)
    {
        return m_controls_store.settle_named(name);
    }

private:
    ControlStore m_controls_store;
};

} // namespace MayaFlux::Nodes::Network
