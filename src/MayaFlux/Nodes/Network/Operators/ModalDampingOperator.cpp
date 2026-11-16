#include "ModalDampingOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Scalar.hpp"
#include "MayaFlux/Nodes/Network/ModalNetwork.hpp"

namespace MayaFlux::Nodes::Network {

bool ModalDampingOperator::valid(const Response& response) noexcept
{
    return std::isfinite(response.quiet_seconds) && response.quiet_seconds > 0.0
        && std::isfinite(response.loud_seconds) && response.loud_seconds > 0.0
        && std::isfinite(response.reference_rms) && response.reference_rms > 0.0
        && std::isfinite(response.smoothing_seconds) && response.smoothing_seconds >= 0.0;
}

ModalDampingOperator::ModalDampingOperator(const std::shared_ptr<ModalNetwork>& network)
    : m_controls(declare_controls<ModalDampingControls>())
    , m_network(network)
{
    if (!network) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "ModalDampingOperator has no ModalNetwork");
        return;
    }

    m_modes.reserve(network->get_node_count());
    for (size_t index = 0; index < network->get_node_count(); ++index) {
        const double decay = network->get_mode_decay_time(index).value_or(m_default_response.quiet_seconds);
        const double initial = std::isfinite(decay) && decay > 0.0
            ? decay
            : m_default_response.quiet_seconds;

        m_modes.push_back(ModeState {
            .response = m_default_response,
            .decay_seconds = initial });
    }
}

void ModalDampingOperator::set_mode(size_t index, const ModalDampingControls& controls)
{
    refresh_controls();
    apply_controls();

    if (index >= m_modes.size()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "ModalDampingOperator ignored mode {}, it has {} modes", index, m_modes.size());
        return;
    }

    auto& mode = m_modes[index];
    const auto field = [&](const Control& control, double Response::* member, std::string_view name) {
        if (!control.has_value() || control.sourced()) {
            return;
        }

        Response candidate = mode.response;
        candidate.*member = control.value();
        if (!valid(candidate)) {
            MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
                "ModalDampingOperator ignored invalid value for {}", name);
            return;
        }
        mode.response = candidate;
    };

    field(controls.quiet_seconds, &Response::quiet_seconds, "quiet_seconds");
    field(controls.loud_seconds, &Response::loud_seconds, "loud_seconds");
    field(controls.reference_rms, &Response::reference_rms, "reference_rms");
    field(controls.smoothing_seconds, &Response::smoothing_seconds, "smoothing_seconds");
}

bool ModalDampingOperator::apply_field(double Response::* member, double value)
{
    Response candidate = m_default_response;
    candidate.*member = value;
    if (!valid(candidate)) {
        return false;
    }

    m_default_response = candidate;
    for (auto& mode : m_modes) {
        mode.response.*member = value;
    }
    return true;
}

void ModalDampingOperator::apply_controls()
{
    const auto field = [this](Control& control, double Response::* member, std::string_view name) {
        const auto value = control.pending();
        if (value && !apply_field(member, *value)) {
            MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
                "ModalDampingOperator ignored invalid value for {}", name);
        }
    };

    field(m_controls.quiet_seconds, &Response::quiet_seconds, "quiet_seconds");
    field(m_controls.loud_seconds, &Response::loud_seconds, "loud_seconds");
    field(m_controls.reference_rms, &Response::reference_rms, "reference_rms");
    field(m_controls.smoothing_seconds, &Response::smoothing_seconds, "smoothing_seconds");
}

void ModalDampingOperator::read_controls(ModalDampingControls& controls) const
{
    controls.quiet_seconds.observe(m_default_response.quiet_seconds);
    controls.loud_seconds.observe(m_default_response.loud_seconds);
    controls.reference_rms.observe(m_default_response.reference_rms);
    controls.smoothing_seconds.observe(m_default_response.smoothing_seconds);
}

void ModalDampingOperator::process(float frames)
{
    refresh_controls();
    apply_controls();

    const auto network = m_network.lock();
    if (!network || network->get_sample_rate() == 0 || frames <= 0.0F) {
        return;
    }

    const double dt = static_cast<double>(frames) / network->get_sample_rate();
    const size_t count = std::min(m_modes.size(), network->get_node_count());

    for (size_t index = 0; index < count; ++index) {
        const auto buffer = network->get_node_audio_buffer(index);
        double sum_squares = 0.0;

        if (buffer) {
            for (const double sample : *buffer) {
                if (std::isfinite(sample)) {
                    sum_squares += sample * sample;
                }
            }
        }

        const double rms = buffer && !buffer->empty()
            ? std::sqrt(sum_squares / static_cast<double>(buffer->size()))
            : 0.0;

        auto& mode = m_modes[index];
        const auto& response = mode.response;
        const double amount = std::clamp(rms / response.reference_rms, 0.0, 1.0);
        const double target = std::lerp(response.quiet_seconds, response.loud_seconds, amount);

        mode.decay_seconds = Kinesis::damp(
            mode.decay_seconds, target, response.smoothing_seconds, dt);
        (void)network->set_mode_decay_time(index, mode.decay_seconds);
    }
}

void ModalDampingOperator::set_parameter(std::string_view name, double value)
{
    if (!set_named_control(name, value)) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "ModalDampingOperator ignored unknown parameter {}", name);
    }
}

std::optional<double> ModalDampingOperator::query_state(std::string_view name) const
{
    if (const auto value = ControlStore::read_named(get(), name)) {
        return value;
    }
    if (name == "mode_count") {
        return static_cast<double>(m_modes.size());
    }
    return std::nullopt;
}

}
