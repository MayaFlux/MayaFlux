#pragma once

namespace MayaFlux::Nodes::Network {

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
};

} // namespace MayaFlux::Nodes::Network
