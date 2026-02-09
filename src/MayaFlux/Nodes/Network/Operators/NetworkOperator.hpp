#pragma once

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
};

} // namespace MayaFlux::Nodes::Network
