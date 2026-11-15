#include "TerminationOperators.hpp"

namespace MayaFlux::Nodes::Network {

namespace Acoustics = Kinesis::Acoustics;

namespace {

    [[nodiscard]] bool grid_usable(const Acoustics::DuctGrid& grid) noexcept
    {
        return grid.intervals >= 2 && grid.pressure_gain.size() == grid.intervals + 1;
    }

    [[nodiscard]] double end_gain_of(const Acoustics::DuctGrid& grid, DuctEnd end) noexcept
    {
        return end == DuctEnd::MOUTH ? grid.pressure_gain.front() : grid.pressure_gain.back();
    }

    [[nodiscard]] double end_radius_of(const Acoustics::DuctGrid& grid, DuctEnd end) noexcept
    {
        return end == DuctEnd::MOUTH ? grid.start_radius : grid.end_radius;
    }

    [[nodiscard]] bool positive_finite(double value) noexcept
    {
        return std::isfinite(value) && value > 0.0;
    }

}

//-----------------------------------------------------------------------------
// Wall
//-----------------------------------------------------------------------------

bool WallOperator::accepts(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& /*spec*/, DuctEnd /*end*/) const
{
    return grid_usable(grid);
}

void WallOperator::prepare(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& /*spec*/, DuctEnd end)
{
    m_end_gain = end_gain_of(grid, end);
}

std::pair<double, double> WallOperator::couple(double end_pressure, double interior_flow)
{
    return { Acoustics::end_cell_pressure(end_pressure, m_end_gain, -interior_flow), 0.0 };
}

//-----------------------------------------------------------------------------
// Reed
//-----------------------------------------------------------------------------

ReedOperator::ReedOperator(double width, double rest_opening, double closing_pressure)
    : m_width(width)
    , m_rest_opening(rest_opening)
    , m_closing_pressure(closing_pressure)
{
}

Acoustics::ReedChannel ReedOperator::channel() const noexcept
{
    return {
        .width = m_width.load(std::memory_order_relaxed),
        .rest_opening = m_rest_opening.load(std::memory_order_relaxed),
        .closing_pressure = m_closing_pressure.load(std::memory_order_relaxed),
        .air_density = m_density
    };
}

bool ReedOperator::accepts(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& spec, DuctEnd end) const
{
    return grid_usable(grid) && positive_finite(spec.air_density)
        && positive_finite(end_gain_of(grid, end)) && positive_finite(grid.time_step)
        && positive_finite(m_width.load(std::memory_order_relaxed))
        && positive_finite(m_rest_opening.load(std::memory_order_relaxed))
        && positive_finite(m_closing_pressure.load(std::memory_order_relaxed));
}

void ReedOperator::prepare(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& spec, DuctEnd end)
{
    m_end_gain = end_gain_of(grid, end);
    m_density = spec.air_density;
    m_time_step = grid.time_step;
    reset();
    process(0.0F);
}

void ReedOperator::reset()
{
    m_pressure = 0.0;
    m_warm_start = 0.0;
    m_motion = { 0.0, 0.0 };
    m_random = 0x9E3779B97F4A7C15ULL;
}

double ReedOperator::next_white() noexcept
{
    m_random ^= m_random >> 12;
    m_random ^= m_random << 25;
    m_random ^= m_random >> 27;
    const uint64_t bits = (m_random * 0x2545F4914F6CDD1DULL) >> 11;
    return static_cast<double>(bits) * (2.0 / 9007199254740992.0) - 1.0;
}

void ReedOperator::process(float /*frames*/)
{
    const double smoothing = m_smoothing.load(std::memory_order_relaxed);
    m_alpha = smoothing > 0.0 ? 1.0 - std::exp(-m_time_step / smoothing) : 1.0;
    m_unique.store(
        m_resonance.load(std::memory_order_relaxed) > 0.0
            || Acoustics::reed_coupling_is_unique(channel(), Acoustics::flow_coupling_gain(m_end_gain)),
        std::memory_order_relaxed);
}

std::pair<double, double> ReedOperator::couple(double end_pressure, double interior_flow)
{
    m_pressure += m_alpha * (m_target_pressure.load(std::memory_order_relaxed) - m_pressure);

    double blow = m_pressure;
    const double noise = m_noise.load(std::memory_order_relaxed);
    if (noise > 0.0) {
        blow *= 1.0 + noise * next_white();
    }

    const double resonance = m_resonance.load(std::memory_order_relaxed);
    if (resonance > 0.0) {
        const auto [pressure, flow, difference] = Acoustics::dynamic_reed_mouth_step(
            channel(), resonance, m_reed_damping.load(std::memory_order_relaxed), m_time_step,
            m_motion, m_end_gain, blow, end_pressure, interior_flow);

        return { pressure, flow };
    }

    const auto [pressure, flow, difference, status] = Acoustics::reed_mouth_step(
        channel(), m_warm_start, m_end_gain, blow, end_pressure, interior_flow);

    return { pressure, flow };
}

void ReedOperator::set_parameter(std::string_view param, double value)
{
    if (!std::isfinite(value)) {
        return;
    }

    if (param == "pressure") {
        m_target_pressure.store(value, std::memory_order_relaxed);
    } else if (param == "smoothing") {
        m_smoothing.store(std::max(0.0, value), std::memory_order_relaxed);
    } else if (param == "resonance") {
        m_resonance.store(std::max(0.0, value), std::memory_order_relaxed);
    } else if (param == "reed_damping") {
        m_reed_damping.store(std::max(0.0, value), std::memory_order_relaxed);
    } else if (param == "noise") {
        m_noise.store(std::max(0.0, value), std::memory_order_relaxed);
    } else if (value > 0.0) {
        if (param == "closing_pressure") {
            m_closing_pressure.store(value, std::memory_order_relaxed);
        } else if (param == "opening") {
            m_rest_opening.store(value, std::memory_order_relaxed);
        } else if (param == "width") {
            m_width.store(value, std::memory_order_relaxed);
        }
    }
}

std::optional<double> ReedOperator::query_state(std::string_view query) const
{
    if (query == "pressure") {
        return m_target_pressure.load(std::memory_order_relaxed);
    }
    if (query == "closing_pressure") {
        return m_closing_pressure.load(std::memory_order_relaxed);
    }
    if (query == "opening") {
        return m_rest_opening.load(std::memory_order_relaxed);
    }
    if (query == "width") {
        return m_width.load(std::memory_order_relaxed);
    }
    if (query == "smoothing") {
        return m_smoothing.load(std::memory_order_relaxed);
    }
    if (query == "resonance") {
        return m_resonance.load(std::memory_order_relaxed);
    }
    if (query == "reed_damping") {
        return m_reed_damping.load(std::memory_order_relaxed);
    }
    if (query == "noise") {
        return m_noise.load(std::memory_order_relaxed);
    }
    if (query == "unique") {
        return m_unique.load(std::memory_order_relaxed) ? 1.0 : 0.0;
    }
    return std::nullopt;
}

//-----------------------------------------------------------------------------
// Radiation
//-----------------------------------------------------------------------------

RadiationOperator::RadiationOperator(Acoustics::RadiationModel model)
    : m_model(model)
{
}

std::optional<Acoustics::RadiationCoefficients> RadiationOperator::coefficients_for(
    const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& spec, DuctEnd end) const
{
    if (!grid_usable(grid)) {
        return std::nullopt;
    }

    const double radius = end_radius_of(grid, end);
    return Acoustics::make_radiation_coefficients(
        { .model = m_model,
            .radius = radius,
            .sound_speed = spec.sound_speed,
            .air_density = spec.air_density,
            .time_step = grid.time_step },
        std::numbers::pi * radius * radius,
        end_gain_of(grid, end));
}

bool RadiationOperator::accepts(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& spec, DuctEnd end) const
{
    return coefficients_for(grid, spec, end).has_value();
}

void RadiationOperator::prepare(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& spec, DuctEnd end)
{
    const auto coefficients = coefficients_for(grid, spec, end);
    m_ready = coefficients.has_value();
    if (m_ready) {
        m_coefficients = *coefficients;
    }
    m_end_gain = end_gain_of(grid, end);
    m_state = {};
}

void RadiationOperator::reset()
{
    m_state = {};
}

std::pair<double, double> RadiationOperator::couple(double end_pressure, double interior_flow)
{
    if (!m_ready) {
        return { Acoustics::end_cell_pressure(end_pressure, m_end_gain, -interior_flow), 0.0 };
    }

    const auto [pressure, outflow] = Acoustics::radiation_step(m_coefficients, m_state, end_pressure, -interior_flow);
    return { pressure, -outflow };
}

//-----------------------------------------------------------------------------
// Junction
//-----------------------------------------------------------------------------

JunctionOperator::JunctionOperator(FlowLaw law, double maximum_flow, double driving_pressure)
    : m_law(std::move(law))
    , m_maximum_flow(maximum_flow)
    , m_driving_pressure(driving_pressure)
{
}

bool JunctionOperator::accepts(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& /*spec*/, DuctEnd end) const
{
    return grid_usable(grid) && m_law && positive_finite(end_gain_of(grid, end))
        && positive_finite(m_maximum_flow.load(std::memory_order_relaxed));
}

void JunctionOperator::prepare(const Acoustics::DuctGrid& grid, const Acoustics::DuctSpec& /*spec*/, DuctEnd end)
{
    m_end_gain = end_gain_of(grid, end);
    reset();
}

void JunctionOperator::reset()
{
    m_guess = 0.0;
    m_status.store(0, std::memory_order_relaxed);
}

std::pair<double, double> JunctionOperator::couple(double end_pressure, double interior_flow)
{
    if (!m_law) {
        return { Acoustics::end_cell_pressure(end_pressure, m_end_gain, -interior_flow), 0.0 };
    }

    const double driving = m_driving_pressure.load(std::memory_order_relaxed);
    const double largest = m_maximum_flow.load(std::memory_order_relaxed);
    const double gain = Acoustics::flow_coupling_gain(m_end_gain);
    const double target = Acoustics::flow_coupling_target(driving, end_pressure, gain, interior_flow);

    const auto law = [this](double difference) -> Acoustics::FlowSample { return m_law(difference); };
    const auto [difference, flow, status] = Acoustics::solve_pressure_flow(
        law, gain, target, target - gain * largest, target + gain * largest, m_guess);

    m_guess = difference;
    m_status.store(static_cast<uint8_t>(status), std::memory_order_relaxed);
    return { Acoustics::centred_end_pressure(driving, difference, end_pressure), flow };
}

void JunctionOperator::set_parameter(std::string_view param, double value)
{
    if (!std::isfinite(value)) {
        return;
    }

    if (param == "driving_pressure") {
        m_driving_pressure.store(value, std::memory_order_relaxed);
    } else if (param == "maximum_flow" && value > 0.0) {
        m_maximum_flow.store(value, std::memory_order_relaxed);
    }
}

std::optional<double> JunctionOperator::query_state(std::string_view query) const
{
    if (query == "driving_pressure") {
        return m_driving_pressure.load(std::memory_order_relaxed);
    }
    if (query == "maximum_flow") {
        return m_maximum_flow.load(std::memory_order_relaxed);
    }
    if (query == "status") {
        return static_cast<double>(m_status.load(std::memory_order_relaxed));
    }
    return std::nullopt;
}

} // namespace MayaFlux::Nodes::Network
