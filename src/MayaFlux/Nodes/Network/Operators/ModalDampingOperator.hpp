#pragma once

#include "NetworkOperator.hpp"

namespace MayaFlux::Nodes::Network {

class ModalNetwork;

/**
 * @struct ModalDampingControls
 * @brief Controls of a ModalDampingOperator, each a number or a mapped node.
 *
 * set() changes that field of every mode and leaves the other fields of each
 * mode as they were. set_mode() changes one mode from the same struct, numbers
 * only. A network source is per element and is not used. A value that would
 * make a response invalid is refused. The defaults are 1 second quiet, 0.2
 * loud, 0.1 reference_rms and 0.05 smoothing.
 */
struct ModalDampingControls {
    Control quiet_seconds;
    Control loud_seconds;
    Control reference_rms;
    Control smoothing_seconds;
};

MF_DESCRIBE(ModalDampingControls)

/**
 * @brief Adjust each ModalNetwork mode's decay from its previous block's RMS.
 *
 * Processing runs before the next block is rendered. A mode at zero RMS uses
 * quiet_seconds; one at or above reference_rms uses loud_seconds. Intermediate
 * levels interpolate between them. The response is smoothed in block time.
 * The network keeps the last applied decay when this operator is removed.
 *
 * @code{.cpp}
 * auto damping = gong->add<ModalDampingOperator>();
 * damping->set({ .quiet_seconds = 3.5, .loud_seconds = 0.18 });
 * damping->set_mode(0, { .quiet_seconds = 7.0, .loud_seconds = 2.5 });
 * @endcode
 */
class MAYAFLUX_API ModalDampingOperator final : public NetworkOperator {
public:
    using Controls = ModalDampingControls;

    explicit ModalDampingOperator(const std::shared_ptr<ModalNetwork>& network);

    /**
     * @brief Change one mode's response from the numbers @p controls holds.
     *
     * A pending set() is applied first, so calls take effect in the order made.
     * An index past the last mode, or a value that would make the response
     * invalid, is ignored with a warning. Like the other mode edits this
     * belongs outside the block.
     */
    void set_mode(size_t index, const ModalDampingControls& controls);

    void process(float frames) override;

    /** @brief Sets the control of that name, so "loud_seconds" is the same as set({ .loud_seconds = ... }). */
    void set_parameter(std::string_view name, double value) override;

    /** @brief The control of that name as the operator holds it, or "mode_count". */
    [[nodiscard]] std::optional<double> query_state(std::string_view name) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "ModalDamping"; }

    /** @brief Fills the controls whose value the operator holds outside the struct. */
    void read_controls(ModalDampingControls& controls) const;

private:
    struct Response {
        double quiet_seconds { 1.0 };
        double loud_seconds { 0.2 };
        double reference_rms { 0.1 };
        double smoothing_seconds { 0.05 };
    };

    struct ModeState {
        Response response;
        double decay_seconds {};
    };

    [[nodiscard]] static bool valid(const Response& response) noexcept;

    bool apply_field(double Response::* member, double value);
    void apply_controls();

    ModalDampingControls& m_controls;
    std::weak_ptr<ModalNetwork> m_network;
    Response m_default_response;
    std::vector<ModeState> m_modes;
};

}
#include "ModalDampingOperator.describe.hpp"
