#pragma once

#include "NodeNetwork.hpp"

namespace MayaFlux::Nodes::Generator {
class Generator;
}

namespace MayaFlux::Nodes::Network {

/**
 * @class ModalNetwork
 * @brief Network of resonant modes for modal synthesis
 *
 * CONCEPT:
 * ========
 * Modal synthesis models physical objects as collections of resonant modes,
 * each with its own frequency, decay rate, and amplitude. The sum of all
 * modes produces rich, organic timbres characteristic of struck/plucked
 * instruments (bells, marimbas, strings, membranes).
 *
 * STRUCTURE:
 * ==========
 * Each mode is an independent oscillator (typically Sine) with:
 * - Frequency: Resonant frequency of the mode
 * - Decay: Exponential amplitude envelope
 * - Amplitude: Initial strike/excitation strength
 *
 * Modes can follow various frequency relationships:
 * - HARMONIC: f, 2f, 3f, 4f... (ideal strings)
 * - INHARMONIC: f, 2.76f, 5.40f, 8.93f... (bells, bars)
 * - STRETCHED: f, 2.01f, 3.02f, 4.04f... (stiff strings, piano)
 * - CUSTOM: User-defined frequency ratios
 *
 * USAGE:
 * ======
 * ```cpp
 * // Bell-like inharmonic spectrum
 * auto bell = std::make_shared<ModalNetwork>(
 *     16,                          // 16 modes
 *     220.0,                       // Base frequency
 *     ModalNetwork::Spectrum::INHARMONIC
 * );
 * bell->set_output_mode(OutputMode::AUDIO_SINK);
 * bell->excite(1.0);  // Strike the bell
 *
 * node_graph_manager->add_network(bell, ProcessingToken::AUDIO_RATE);
 * ```
 *
 * PARAMETER MAPPING:
 * ==================
 * External nodes can control:
 * - "frequency": Base frequency (BROADCAST)
 * - "decay": Global decay multiplier (BROADCAST)
 * - "amplitude": Per-mode amplitude (ONE_TO_ONE)
 * - "detune": Per-mode frequency offset (ONE_TO_ONE)
 */
class MAYAFLUX_API ModalNetwork : public NodeNetwork {
public:
    /**
     * @enum Spectrum
     * @brief Predefined frequency relationship patterns
     */
    enum class Spectrum : uint8_t {
        HARMONIC, ///< Integer harmonics: f, 2f, 3f, 4f...
        INHARMONIC, ///< Bell-like: f, 2.76f, 5.40f, 8.93f, 13.34f...
        STRETCHED, ///< Piano-like stiffness: f, 2.01f, 3.02f, 4.04f...
        CUSTOM ///< User-provided frequency ratios
    };

    /**
     * @struct ModalNode
     * @brief Represents a single resonant mode
     */
    struct ModalNode {
        std::shared_ptr<Generator::Generator> oscillator; ///< Sine wave generator

        double base_frequency; ///< Frequency without modulation
        double current_frequency; ///< After mapping/modulation
        double frequency_ratio; ///< Ratio relative to fundamental

        double decay_time; ///< Time constant for amplitude decay (seconds)
        double amplitude; ///< Current amplitude (0.0 to 1.0)
        double initial_amplitude; ///< Amplitude at excitation
        double decay_coefficient; ///< Precomputed exp factor

        double phase = 0.0; ///< Current phase (for manual oscillator impl)
        size_t index; ///< Index in network
    };

    //-------------------------------------------------------------------------
    // Construction
    //-------------------------------------------------------------------------

    /**
     * @brief Create modal network with predefined spectrum
     * @param num_modes Number of resonant modes
     * @param fundamental Base frequency in Hz
     * @param spectrum Frequency relationship pattern
     * @param base_decay Base decay time in seconds (modes get proportional decay)
     */
    ModalNetwork(size_t num_modes, double fundamental = 220.0,
        Spectrum spectrum = Spectrum::HARMONIC, double base_decay = 1.0);

    /**
     * @brief Create modal network with custom frequency ratios
     * @param frequency_ratios Vector of frequency multipliers relative to
     * fundamental
     * @param fundamental Base frequency in Hz
     * @param base_decay Base decay time in seconds
     */
    ModalNetwork(const std::vector<double>& frequency_ratios,
        double fundamental = 220.0, double base_decay = 1.0);

    //-------------------------------------------------------------------------
    // NodeNetwork Interface Implementation
    //-------------------------------------------------------------------------

    void process_batch(unsigned int num_samples) override;

    [[nodiscard]] size_t get_node_count() const override
    {
        return m_modes.size();
    }

    void initialize() override { };

    void reset() override;

    [[nodiscard]] std::unordered_map<std::string, std::string>
    get_metadata() const override;

    //-------------------------------------------------------------------------
    // Parameter Mapping
    //-------------------------------------------------------------------------

    void map_parameter(const std::string& param_name,
        const std::shared_ptr<Node>& source,
        MappingMode mode = MappingMode::BROADCAST) override;

    void
    map_parameter(const std::string& param_name,
        const std::shared_ptr<NodeNetwork>& source_network) override;

    void unmap_parameter(const std::string& param_name) override;

    //-------------------------------------------------------------------------
    // Modal Control
    //-------------------------------------------------------------------------

    /**
     * @brief Excite all modes (strike/pluck)
     * @param strength Excitation strength (0.0 to 1.0+)
     *
     * Resets all mode amplitudes to their initial values scaled by strength.
     * Simulates striking or plucking the resonant structure.
     */
    void excite(double strength = 1.0);

    /**
     * @brief Excite specific mode
     * @param mode_index Index of mode to excite
     * @param strength Excitation strength
     */
    void excite_mode(size_t mode_index, double strength = 1.0);

    /**
     * @brief Damp all modes (rapidly reduce amplitude)
     * @param damping_factor Multiplier applied to current amplitudes (0.0 to 1.0)
     */
    void damp(double damping_factor = 0.1);

    /**
     * @brief Set base frequency (fundamental)
     * @param frequency Frequency in Hz
     *
     * Updates all mode frequencies proportionally to maintain spectrum shape.
     */
    void set_fundamental(double frequency);

    /**
     * @brief Get current fundamental frequency
     */
    [[nodiscard]] double get_fundamental() const { return m_fundamental; }

    /**
     * @brief Set global decay multiplier
     * @param multiplier Scales all decay times (>1.0 = longer decay, <1.0 =
     * shorter)
     */
    void set_decay_multiplier(double multiplier)
    {
        m_decay_multiplier = multiplier;
    }

    /**
     * @brief Get mode data (read-only access for visualization)
     */
    [[nodiscard]] const std::vector<ModalNode>& get_modes() const
    {
        return m_modes;
    }

    /**
     * @brief Get specific mode
     */
    [[nodiscard]] const ModalNode& get_mode(size_t index) const
    {
        return m_modes.at(index);
    }

    /**
     * @brief Get output of specific internal node (for ONE_TO_ONE mapping)
     * @param index Index of node in network
     * @return Output value, or nullopt if not applicable
     */
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;

private:
    //-------------------------------------------------------------------------
    // Internal State
    //-------------------------------------------------------------------------

    std::vector<ModalNode> m_modes;
    Spectrum m_spectrum;
    double m_fundamental;
    double m_decay_multiplier = 1.0;

    // Cached output for get_audio_output()
    mutable double m_last_output = 0.0;

    //-------------------------------------------------------------------------
    // Initialization Helpers
    //-------------------------------------------------------------------------

    /**
     * @brief Generate frequency ratios for predefined spectra
     */
    static std::vector<double> generate_spectrum_ratios(Spectrum spectrum,
        size_t count);

    /**
     * @brief Initialize modes with given frequency ratios
     */
    void initialize_modes(const std::vector<double>& ratios, double base_decay);

    /**
     * @brief Update mapped parameters before processing
     */
    void update_mapped_parameters();

    /**
     * @brief Apply broadcast parameter to all modes
     */
    void apply_broadcast_parameter(const std::string& param, double value);

    /**
     * @brief Apply one-to-one parameter from another network
     */
    void apply_one_to_one_parameter(const std::string& param,
        const std::shared_ptr<NodeNetwork>& source);
};

} // namespace MayaFlux::Nodes::Network
