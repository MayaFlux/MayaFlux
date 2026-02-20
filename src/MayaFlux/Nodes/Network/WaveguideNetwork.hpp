#pragma once

#include "NodeNetwork.hpp"

#include "MayaFlux/Kinesis/Stochastic.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Nodes::Generator {
class Generator;
}

namespace MayaFlux::Nodes::Filters {
class Filter;
}

namespace MayaFlux::Nodes::Network {

/**
 * @class WaveguideNetwork
 * @brief Digital waveguide synthesis via uni- and bidirectional delay-line architectures
 *
 * CONCEPT:
 * ========
 * Digital waveguide synthesis models vibrating structures as traveling waves
 * propagating through delay lines. A loop filter at each termination simulates
 * frequency-dependent energy loss. This complements ModalNetwork (frequency-domain)
 * with time-domain physical modeling: where ModalNetwork decomposes resonance into
 * independent modes, WaveguideNetwork simulates wave propagation directly.
 *
 * PROPAGATION MODES:
 * ==================
 * WaveguideSegment is direction-agnostic and supports both modes via PropagationMode:
 *
 * UNIDIRECTIONAL (STRING):
 *   Single loop. Wave circulates on p_plus only. Karplus-Strong extended model.
 *   Loop filter at the single termination controls frequency-dependent damping.
 *
 *     exciter ──► p_plus ──► [delay N] ──► loop_filter ──► loss ──┐
 *     output  ◄── tap(pickup_sample)                               │
 *                 └─────────────────────────────────────────────────┘
 *
 * BIDIRECTIONAL (TUBE):
 *   Two rails. p_plus travels toward the open end, p_minus returns toward the
 *   closed end. Reflection sign at each termination determines harmonic series:
 *   - Closed end (mouthpiece): pressure node, sign preserved → odd harmonics
 *   - Open end (bell): pressure antinode, sign inverted → adds even harmonics
 *   Output is the physical pressure sum p_plus[pickup] + p_minus[pickup].
 *
 *     exciter ──► p_plus  ──► [delay N] ──► loop_filter ──► loss ──► open-end  (−)
 *     output  ◄── tap                                                     │
 *                 p_minus ◄── [delay N] ◄── loop_filter ◄── loss ◄── closed-end (+)
 *
 * EXCITATION:
 * ===========
 * pluck() seeds p_plus with a triangle waveform (shaped initial displacement).
 * strike() seeds p_plus with a Gaussian-windowed noise burst at the strike point.
 * Both clear p_minus, ensuring a clean bidirectional state on re-excitation.
 * Continuous and sample-based exciters inject per-sample into p_plus at the
 * closed end on every call to process_batch().
 *
 * USAGE:
 * ======
 * ```cpp
 * // Plucked string
 * auto string = std::make_shared<WaveguideNetwork>(
 *     WaveguideNetwork::WaveguideType::STRING, 220.0);
 * string->pluck(0.3, 0.8);
 *
 * // Cylindrical bore (clarinet-like, odd harmonics)
 * auto tube = std::make_shared<WaveguideNetwork>(
 *     WaveguideNetwork::WaveguideType::TUBE, 220.0);
 * tube->strike(0.1, 0.9);
 *
 * // Via vega API:
 * auto string = vega.WaveguideNetwork(WaveguideType::STRING, 220.0)[0] | Audio;
 * string->pluck(0.3, 0.8);
 * ```
 *
 * PARAMETER MAPPING:
 * ==================
 * - "frequency": Fundamental frequency in Hz (BROADCAST)
 * - "damping" / "loss": Loop filter cutoff / loss factor (BROADCAST)
 * - "position": Pickup position along the delay line (BROADCAST)
 */
class MAYAFLUX_API WaveguideNetwork : public NodeNetwork {
public:
    /**
     * @enum WaveguideType
     * @brief Physical structure being modeled
     */
    enum class WaveguideType : uint8_t {
        STRING, ///< 1D string (Karplus-Strong extended)
        TUBE, ///< Cylindrical bore (future: clarinet, flute)
    };

    /**
     * @enum ExciterType
     * @brief Excitation signal types for waveguide synthesis
     */
    enum class ExciterType : uint8_t {
        IMPULSE, ///< Single-sample Dirac impulse
        NOISE_BURST, ///< Short white noise burst (default for pluck)
        FILTERED_NOISE, ///< Spectrally-shaped noise burst
        SAMPLE, ///< User-provided excitation waveform
        CONTINUOUS ///< External node as continuous exciter (bowing)
    };

    /**
     * @struct WaveguideSegment
     * @brief 1D delay-line segment supporting both uni- and bidirectional propagation
     *
     * UNIDIRECTIONAL (STRING):
     *   Only p_plus is active. Wave circulates in a single loop.
     *   p_minus is allocated but never written or read.
     *   Output tapped at pickup_sample along p_plus.
     *
     * BIDIRECTIONAL (TUBE):
     *   Both rails active. p_plus travels toward the open end (bell),
     *   p_minus travels back toward the mouthpiece.
     *   At the closed end:  p_minus[0] = -p_plus[end]  (pressure inversion)
     *   At the open end:    p_plus[0]  = -p_minus[end]  (approximate open reflection)
     *   Output is p_plus[pickup] + p_minus[pickup] (physical pressure sum).
     *
     * Both rails share the same integer/fractional delay length and loop filter.
     * The propagation mode is set once at construction and never changes.
     */
    struct WaveguideSegment {
        /**
         * @enum PropagationMode
         * @brief Whether this segment uses one or two traveling-wave rails
         */
        enum class PropagationMode : uint8_t {
            UNIDIRECTIONAL, ///< Single loop (STRING)
            BIDIRECTIONAL, ///< Forward + backward rails (TUBE)
        };

        Memory::HistoryBuffer<double> p_plus; ///< Forward-traveling wave rail
        Memory::HistoryBuffer<double> p_minus; ///< Backward-traveling wave rail (BIDIRECTIONAL only)
        std::shared_ptr<Filters::Filter> loop_filter; ///< UNIDIRECTIONAL: single termination filter
        std::shared_ptr<Filters::Filter> loop_filter_closed; ///< BIDIRECTIONAL: closed-end filter (mouthpiece/nut)
        std::shared_ptr<Filters::Filter> loop_filter_open; ///< BIDIRECTIONAL: open-end filter (bell/bridge)
        PropagationMode mode { PropagationMode::UNIDIRECTIONAL };
        double loss_factor { 0.996 };

        /**
         * @brief Construct segment with both rails at the specified length
         * @param length  Delay-line length in samples
         * @param prop_mode  Propagation mode; determines which rails are active
         *
         * Both rails are always allocated regardless of mode to avoid
         * conditional sizing logic at call sites. The UNIDIRECTIONAL path
         * simply never touches p_minus.
         */
        explicit WaveguideSegment(size_t length,
            PropagationMode prop_mode = PropagationMode::UNIDIRECTIONAL)
            : p_plus(length)
            , p_minus(length)
            , mode(prop_mode)
        {
        }
    };

    //-------------------------------------------------------------------------
    // Construction
    //-------------------------------------------------------------------------

    /**
     * @brief Create waveguide network with specified type and frequency
     * @param type Physical structure to model
     * @param fundamental_freq Fundamental frequency in Hz
     * @param sample_rate Sample rate in Hz
     */
    WaveguideNetwork(
        WaveguideType type,
        double fundamental_freq,
        double sample_rate = 48000.0);

    //-------------------------------------------------------------------------
    // NodeNetwork Interface
    //-------------------------------------------------------------------------

    void process_batch(unsigned int num_samples) override;

    [[nodiscard]] size_t get_node_count() const override { return m_segments.size(); }

    void initialize() override;
    void reset() override;

    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;
    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

    //-------------------------------------------------------------------------
    // Parameter Mapping
    //-------------------------------------------------------------------------

    void map_parameter(const std::string& param_name,
        const std::shared_ptr<Node>& source,
        MappingMode mode = MappingMode::BROADCAST) override;

    void map_parameter(const std::string& param_name,
        const std::shared_ptr<NodeNetwork>& source) override;

    void unmap_parameter(const std::string& param_name) override;

    //-------------------------------------------------------------------------
    // Excitation
    //-------------------------------------------------------------------------

    /**
     * @brief Pluck the string at a normalized position
     * @param position Normalized position along string (0.0 to 1.0)
     * @param strength Excitation amplitude (0.0 to 1.0+)
     *
     * Fills the delay line with a shaped noise burst. Position affects
     * spectral content: 0.5 = center (warm), near 0/1 = bridge (bright).
     */
    void pluck(double position = 0.5, double strength = 1.0);

    /**
     * @brief Strike the string/tube with an impulse
     * @param position Normalized strike position
     * @param strength Excitation amplitude
     */
    void strike(double position = 0.5, double strength = 1.0);

    /**
     * @brief Set exciter type
     */
    void set_exciter_type(ExciterType type) { m_exciter_type = type; }

    /**
     * @brief Get current exciter type
     */
    [[nodiscard]] ExciterType get_exciter_type() const { return m_exciter_type; }

    /**
     * @brief Set noise burst duration for exciter
     * @param seconds Duration in seconds
     */
    void set_exciter_duration(double seconds);

    /**
     * @brief Set filter for shaped noise excitation
     * @param filter Filter node for spectral shaping (FILTERED_NOISE only)
     */
    void set_exciter_filter(const std::shared_ptr<Filters::Filter>& filter) { m_exciter_filter = filter; }

    /**
     * @brief Set custom excitation waveform
     * @param sample Excitation waveform (SAMPLE only)
     */
    void set_exciter_sample(const std::vector<double>& sample);

    /**
     * @brief Set continuous exciter node (for bowing/blowing)
     * @param node Source node providing continuous excitation
     */
    void set_exciter_node(const std::shared_ptr<Node>& node) { m_exciter_node = node; }

    //-------------------------------------------------------------------------
    // Waveguide Control
    //-------------------------------------------------------------------------

    /**
     * @brief Set fundamental frequency
     * @param freq Frequency in Hz
     *
     * Recomputes delay line length. Fractional part handled via interpolation.
     */
    void set_fundamental(double freq);

    /**
     * @brief Get current fundamental frequency
     */
    [[nodiscard]] double get_fundamental() const { return m_fundamental; }

    /**
     * @brief Set per-sample energy loss factor
     * @param loss Factor applied per sample (0.99-1.0 typical)
     *
     * Controls overall decay time. Values closer to 1.0 sustain longer.
     */
    void set_loss_factor(double loss);

    /**
     * @brief Get current loss factor
     */
    [[nodiscard]] double get_loss_factor() const;

    /**
     * @brief Replace the loop filter
     * @param filter IIR or FIR filter applied in the feedback loop
     *
     * Default is a one-pole averaging filter: y[n] = 0.5*(x[n] + x[n-1])
     * which simulates frequency-dependent string damping.
     */
    void set_loop_filter(const std::shared_ptr<Filters::Filter>& filter);

    /**
     * @brief Set pickup position along the string
     * @param position Normalized position (0.0 to 1.0)
     *
     * Determines where the output is read from the delay line.
     * Different positions emphasize different harmonics.
     */
    void set_pickup_position(double position);

    /**
     * @brief Get current pickup position
     */
    [[nodiscard]] double get_pickup_position() const;

    /**
     * @brief Get waveguide type
     */
    [[nodiscard]] WaveguideType get_type() const { return m_type; }

    /**
     * @brief Get read-only access to segments
     */
    [[nodiscard]] const std::vector<WaveguideSegment>& get_segments() const { return m_segments; }

    /**
     * @brief Set filter for the closed-end termination (mouthpiece/nut)
     * @param filter Filter applied to p_plus as it reflects at the closed end
     *
     * Only meaningful for WaveguideType::TUBE. Falls back to loop_filter if unset.
     * Models reed/embouchure losses and input impedance characteristic.
     */
    void set_loop_filter_closed(const std::shared_ptr<Filters::Filter>& filter);

    /**
     * @brief Set filter for the open-end termination (bell/bridge)
     * @param filter Filter applied to p_minus as it reflects at the open end
     *
     * Only meaningful for WaveguideType::TUBE. Falls back to loop_filter if unset.
     * Models radiation resistance and bell flare HF rolloff.
     */
    void set_loop_filter_open(const std::shared_ptr<Filters::Filter>& filter);

private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------

    WaveguideType m_type;
    double m_fundamental;

    std::vector<WaveguideSegment> m_segments;

    size_t m_delay_length_integer { 0 };
    double m_delay_length_fraction { 0.0 };
    size_t m_pickup_sample { 0 };

    //-------------------------------------------------------------------------
    // Exciter State
    //-------------------------------------------------------------------------

    ExciterType m_exciter_type { ExciterType::NOISE_BURST };
    double m_exciter_duration { 0.005 };
    std::vector<double> m_exciter_sample;
    std::shared_ptr<Filters::Filter> m_exciter_filter;
    std::shared_ptr<Node> m_exciter_node;

    size_t m_exciter_sample_position {};
    bool m_exciter_active {};
    size_t m_exciter_samples_remaining {};

    //-------------------------------------------------------------------------
    // Output
    //-------------------------------------------------------------------------

    mutable double m_last_output {};

    Kinesis::Stochastic::Stochastic m_random_generator;

    //-------------------------------------------------------------------------
    // Internal Methods
    //-------------------------------------------------------------------------

    void compute_delay_length();
    void create_default_loop_filter();

    /**
     * @brief Read from delay line with linear fractional interpolation
     * @param delay History buffer to read from
     * @param integer_part Integer delay in samples
     * @param fraction Fractional delay (0.0 to 1.0)
     * @return Interpolated sample value
     */
    [[nodiscard]] double read_with_interpolation(
        const Memory::HistoryBuffer<double>& delay,
        size_t integer_part,
        double fraction) const;

    double generate_exciter_sample();
    void initialize_exciter();

    void update_mapped_parameters();
    void apply_broadcast_parameter(const std::string& param, double value);
    void apply_one_to_one_parameter(const std::string& param, const std::shared_ptr<NodeNetwork>& source);

    void process_unidirectional(WaveguideSegment& seg, unsigned int num_samples);
    void process_bidirectional(WaveguideSegment& seg, unsigned int num_samples);
};

} // namespace MayaFlux::Nodes::Network
