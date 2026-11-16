#pragma once

#include "NetworkOperator.hpp"

#include "MayaFlux/Kinesis/Acoustics/DuctGrid.hpp"
#include "MayaFlux/Kinesis/Acoustics/PressureFlowSolve.hpp"
#include "MayaFlux/Kinesis/Acoustics/Radiation.hpp"
#include "MayaFlux/Kinesis/Acoustics/Reed.hpp"
#include "MayaFlux/Kinesis/Acoustics/StaggeredLine.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @enum DuctEnd
 * @brief Which end of a duct an operator terminates.
 */
enum class DuctEnd : uint8_t {
    MOUTH,
    BELL
};

/**
 * @class TerminationOperator
 * @brief What closes one end of a DuctNetwork: a wall, a reed, a radiating
 *        opening, or any pressure dependent flow.
 *
 * A termination couples the end cell to a flow within the same sample, which a
 * per-block process() cannot do, so it has a per-sample couple() that the
 * network calls directly. process() is left for block housekeeping.
 *
 * Both ends are described in their own inward frame: interior_flow is the
 * volume velocity leaving the end cell into the duct, and the returned flow is
 * the volume velocity entering the duct through the end face. One operator
 * therefore serves either end, and a bell that radiates reports a negative
 * inflow while sound leaves.
 *
 * Parameters are atomics or set between blocks. couple() runs on the audio
 * thread, and so does any callable an operator holds. Replacing an operator on
 * a network happens outside process_batch(), through the network.
 */
class MAYAFLUX_API TerminationOperator : public NetworkOperator {
public:
    /**
     * @brief Whether prepare would succeed for this grid and end.
     * @note Safe from any thread, and does not change the operator.
     */
    [[nodiscard]] virtual bool accepts(
        const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec,
        DuctEnd end) const
        = 0;

    /**
     * @brief Bind to a grid and end, and reset the state.
     * @note Allocates nothing. Called by the network on the audio thread when
     *       a grid is installed, after accepts returned true.
     */
    virtual void prepare(
        const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec,
        DuctEnd end)
        = 0;

    /**
     * @brief Bind to a new grid of the same duct while it keeps sounding.
     * @note Allocates nothing. Called instead of prepare when the shape,
     *       losses or mouth volume change, so a slide of length does not
     *       restart the sound. The default is prepare, which resets.
     */
    virtual void retarget(
        const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec,
        DuctEnd end)
    {
        prepare(grid, spec, end);
    }

    /**
     * @brief Advance the end cell and its coupling by one sample.
     * @param end_pressure Pressure of the end cell at the start of the sample
     * @param interior_flow Volume velocity leaving the end cell into the duct,
     *        from the same sample
     * @return The end pressure after the sample, then the volume velocity
     *         entering the duct through the end face
     */
    [[nodiscard]] virtual std::pair<double, double> couple(double end_pressure, double interior_flow) = 0;

    /** @brief Clear the state, keeping the parameters. */
    virtual void reset() { }

    void process(float /*frames*/) override { }
    void set_parameter(std::string_view /*param*/, double /*value*/) override { }
    [[nodiscard]] std::optional<double> query_state(std::string_view /*query*/) const override { return std::nullopt; }
};

/**
 * @class WallOperator
 * @brief A rigid closed end: no flow through the face.
 */
class MAYAFLUX_API WallOperator final : public TerminationOperator {
public:
    [[nodiscard]] bool accepts(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) const override;
    void prepare(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) override;
    [[nodiscard]] std::pair<double, double> couple(double end_pressure, double interior_flow) override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Wall"; }

private:
    double m_end_gain {};
};

/**
 * @struct ReedControls
 * @brief Controls of a ReedOperator, each a number or a mapped node.
 *
 * Each is checked as the parameter of the same name always was: non negative
 * where it cannot be negative, lip between 0 and 1, and closing_pressure,
 * opening and width only when positive. A value that fails is left out. A
 * network source is per element and is not used.
 */
struct ReedControls {
    Control pressure;
    Control smoothing;
    Control resonance;
    Control reed_damping;
    Control noise;
    Control noise_cutoff;
    Control reed_area;
    Control lip;
    Control closing_pressure;
    Control opening;
    Control width;
};

MF_DESCRIBE(ReedControls)

/**
 * @struct JunctionControls
 * @brief Controls of a JunctionOperator, each a number or a mapped node.
 *
 * maximum_flow is only taken when positive.
 */
struct JunctionControls {
    Control driving_pressure;
    Control maximum_flow;
};

MF_DESCRIBE(JunctionControls)

/**
 * @class ReedOperator
 * @brief Quasi-static single reed driven by a blowing pressure.
 *
 * Parameters: "pressure" (Pa, smoothed per sample), "closing_pressure",
 * "opening", "width" and "smoothing" (seconds of the blowing pressure's
 * one pole smoothing, zero for none). query_state also answers "unique": 1
 * while the coupling has a single solution for the current values, which is
 * checked at the start of each block. When it is 0 the solution follows the
 * branch of the previous sample.
 *
 * Four more give the reed a life of its own. "resonance" (Hz) turns the
 * rigid reed into a damped oscillator at that frequency, with "reed_damping"
 * (1/s, 15000 by default) as its loss; zero resonance, the default, keeps the
 * quasi-static reed. A reed with dynamics puts a formant at its resonance,
 * shapes the attack, and is unique for every value. With the damping below
 * about 5000 and a resonance near 2500 the reed sustains its own oscillation,
 * a squeal near the resonance, instead of the note of the duct.
 *
 * "reed_area" (m^2, zero by default) adds the air the moving reed pushes,
 * minus that area times the reed's velocity, to the flow into the duct;
 * 1.46e-4 is the value used by Avanzini and Rocchesso for a clarinet. "lip"
 * (0 to 1) is the embouchure, a lip pressed against the reed: at 1 the rest
 * opening is reduced by a fifth, the resonance and the closing pressure rise by
 * 40 percent and the damping doubles. A narrower opening lowers the loop gain,
 * so a note near its threshold goes quiet as the lip closes unless the blowing
 * pressure rises with it.
 * The coefficients are a first approximation.
 *
 * "noise" is the breath: an extra flow into the duct, with an rms of that
 * fraction of the reed's own flow, so it swells and fades with the note and is
 * silent when the reed is shut. Its colour is a one pole low-pass at
 * "noise_cutoff" (Hz, 3000 by default). A few hundredths is a hint of breath,
 * a few tenths a breathy note. The noise is a fixed, repeatable sequence.
 */
class MAYAFLUX_API ReedOperator final : public TerminationOperator {
public:
    using Controls = ReedControls;

    explicit ReedOperator(double width = 0.012, double rest_opening = 4.0e-4, double closing_pressure = 3000.0);

    [[nodiscard]] bool accepts(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) const override;
    void prepare(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) override;
    void retarget(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) override;
    [[nodiscard]] std::pair<double, double> couple(double end_pressure, double interior_flow) override;
    void reset() override;

    void process(float frames) override;
    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Reed"; }

    /** @brief Fills every control with the value the reed holds now. */
    void read_controls(ReedControls& controls) const;

private:
    ReedControls& m_controls;

    void apply_controls();

    [[nodiscard]] Kinesis::Acoustics::ReedChannel channel() const noexcept;
    [[nodiscard]] double next_white() noexcept;

    std::atomic<double> m_target_pressure { 0.0 };
    std::atomic<double> m_width;
    std::atomic<double> m_rest_opening;
    std::atomic<double> m_closing_pressure;
    std::atomic<double> m_smoothing { 0.005 };
    std::atomic<double> m_resonance { 0.0 };
    std::atomic<double> m_reed_damping { 15000.0 };
    std::atomic<double> m_noise { 0.0 };
    std::atomic<double> m_noise_cutoff { 3000.0 };
    std::atomic<double> m_reed_area { 0.0 };
    std::atomic<double> m_lip { 0.0 };
    std::atomic<bool> m_unique { true };

    double m_end_gain {};
    double m_density { 1.2 };
    double m_time_step {};
    double m_alpha { 1.0 };
    double m_noise_alpha { 1.0 };
    double m_noise_scale { 1.0 };
    double m_noise_state {};
    double m_last_flow {};
    double m_pressure {};
    double m_warm_start {};
    std::array<double, 2> m_motion {};
    uint64_t m_random { 0x9E3779B97F4A7C15ULL };
};

/**
 * @class RadiationOperator
 * @brief An open end radiating into free space, as a passive second order load.
 */
class MAYAFLUX_API RadiationOperator final : public TerminationOperator {
public:
    /**
     * @param model Network constants, unflanged by default
     */
    explicit RadiationOperator(
        Kinesis::Acoustics::RadiationModel model = Kinesis::Acoustics::unflanged_radiation);

    [[nodiscard]] bool accepts(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) const override;
    void prepare(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) override;
    void retarget(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) override;
    [[nodiscard]] std::pair<double, double> couple(double end_pressure, double interior_flow) override;
    void reset() override;

    [[nodiscard]] std::string_view get_type_name() const override { return "Radiation"; }

private:
    [[nodiscard]] std::optional<Kinesis::Acoustics::RadiationCoefficients> coefficients_for(
        const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec,
        DuctEnd end) const;

    Kinesis::Acoustics::RadiationModel m_model;
    Kinesis::Acoustics::RadiationCoefficients m_coefficients {};
    Kinesis::Acoustics::RadiationState m_state {};
    double m_end_gain {};
    bool m_ready { false };
};

/**
 * @class JunctionOperator
 * @brief Any flow that depends on the pressure difference across it.
 *
 * The flow law maps a pressure difference, driving pressure minus end
 * pressure, to a FlowSample of flow and slope. It runs on the audio thread
 * and must not throw. The law must make d + gain * flow(d) increasing, and
 * its flow must stay within maximum_flow, which sets the bracket of the
 * solve. Parameters: "driving_pressure" and "maximum_flow". query_state
 * "status" answers the last solve's RootStatus as a number.
 */
class MAYAFLUX_API JunctionOperator final : public TerminationOperator {
public:
    using FlowLaw = std::function<Kinesis::Acoustics::FlowSample(double)>;

    /**
     * @param law Flow and slope for a pressure difference
     * @param maximum_flow Largest flow magnitude the law can give
     * @param driving_pressure Pressure on the far side of the junction
     */
    using Controls = JunctionControls;

    JunctionOperator(FlowLaw law, double maximum_flow, double driving_pressure = 0.0);

    [[nodiscard]] bool accepts(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) const override;
    void prepare(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) override;
    void retarget(const Kinesis::Acoustics::DuctGrid& grid,
        const Kinesis::Acoustics::DuctSpec& spec, DuctEnd end) override;
    [[nodiscard]] std::pair<double, double> couple(double end_pressure, double interior_flow) override;
    void reset() override;

    void process(float frames) override;
    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Junction"; }

    /** @brief Fills every control with the value the junction holds now. */
    void read_controls(JunctionControls& controls) const;

private:
    JunctionControls& m_controls;

    void apply_controls();

    FlowLaw m_law;
    std::atomic<double> m_maximum_flow;
    std::atomic<double> m_driving_pressure;
    std::atomic<uint8_t> m_status { 0 };

    double m_end_gain {};
    double m_guess {};
};

} // namespace MayaFlux::Nodes::Network
#include "TerminationOperators.describe.hpp"
