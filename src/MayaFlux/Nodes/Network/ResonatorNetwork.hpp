#pragma once

#include "NodeNetwork.hpp"

#include "MayaFlux/Nodes/Filters/IIR.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class ResonatorNetwork
 * @brief Network of IIR biquad bandpass filters driven by external excitation
 *
 * CONCEPT:
 * ========
 * ResonatorNetwork implements N second-order IIR bandpass sections (biquads), each
 * tuned to an independent centre frequency and Q factor. Unlike ModalNetwork,
 * which synthesises resonance through decaying sinusoidal oscillators in the
 * frequency domain, ResonatorNetwork operates purely in the time domain: it shapes
 * the spectrum of whatever signal is injected into it. Feed white noise and the
 * Network becomes a formant synthesiser; feed a pitched glottal pulse and it voices
 * a vowel; feed an arbitrary signal and it performs spectral morphing toward the
 * target formant profile.
 *
 * Each resonator computes the RBJ Audio EQ cookbook bilinear-transform biquad
 * bandpass (constant 0 dB peak gain form):
 *
 *   H(z) = (1 - z^{-2}) * (b0/a0) / (1 + (a1/a0)*z^{-1} + (a2/a0)*z^{-2})
 *
 * Coefficients derived at construction and on every parameter change:
 *
 *   w0    = 2π f0 / Fs
 *   alpha = sin(w0) / (2 Q)
 *   b0    =  sin(w0) / 2 =  alpha
 *   b1    =  0
 *   b2    = -sin(w0) / 2 = -alpha
 *   a0    =  1 + alpha
 *   a1    = -2 cos(w0)
 *   a2    =  1 - alpha
 *
 * EXCITATION:
 * ===========
 * A single shared exciter node can drive all resonators simultaneously (the
 * classic formant synthesis topology). Alternatively, per-resonator exciter
 * nodes allow independent spectral injection. If no exciter is set, the network
 * accepts external samples passed directly to process_sample() / process_batch().
 *
 * PARAMETER MAPPING:
 * ==================
 * Supports BROADCAST and ONE_TO_ONE modes via the NodeNetwork interface:
 * - "frequency"  — centre frequency of all resonators (BROADCAST) or per-resonator (ONE_TO_ONE)
 * - "q"          — bandwidth/resonance of all resonators (BROADCAST) or per-resonator (ONE_TO_ONE)
 * - "gain"       — amplitude scale per resonator (BROADCAST) or per-resonator (ONE_TO_ONE)
 *
 * OUTPUT:
 * =======
 * Each process_batch() call sums all resonator outputs into a single mixed audio
 * buffer, normalised by the number of active resonators to prevent clipping.
 * Alternatively, get_node_output(index) exposes the most recent individual
 * resonator output for cross-domain routing.
 *
 * USAGE:
 * ======
 * @code
 * // Five-formant vowel synthesiser fed by a noise source
 * auto network= std::make_shared<ResonatorNetwork>(5,
 *     ResonatorNetwork::FormantPreset::VOWEL_A, 48000.0);
 *
 * auto noise = vega.Random(GAUSSIAN);
 * network->set_exciter(noise);
 *
 * auto rb = vega.ResonatorNetwork(5, ResonatorNetwork::FormantPreset::VOWEL_A)[0] | Audio;
 * rb->set_exciter(noise);
 *
 * // Direct frequency/Q control
 * network->set_frequency(2, 2400.0);
 * network->set_q(2, 80.0);
 * @endcode
 */
class MAYAFLUX_API ResonatorNetwork : public NodeNetwork {
public:
    //-------------------------------------------------------------------------
    // Presets
    //-------------------------------------------------------------------------

    /**
     * @enum FormantPreset
     * @brief Common vowel and spectral formant configurations
     *
     * Provides a starting point for formant synthesis. Frequencies are
     * approximate averages for a neutral adult voice; Q values model typical
     * bandwidths (narrower for higher formants).
     */
    enum class FormantPreset : uint8_t {
        NONE, ///< No preset — all resonators initialised at 440 Hz, Q = 10
        VOWEL_A, ///< Open vowel /a/ (F1≈800, F2≈1200, F3≈2500, F4≈3500, F5≈4500 Hz)
        VOWEL_E, ///< Front vowel /e/ (F1≈400, F2≈2000, F3≈2600, F4≈3500, F5≈4500 Hz)
        VOWEL_I, ///< Close front vowel /i/ (F1≈270, F2≈2300, F3≈3000, F4≈3500, F5≈4500 Hz)
        VOWEL_O, ///< Back vowel /o/ (F1≈500, F2≈900, F3≈2500, F4≈3500, F5≈4500 Hz)
        VOWEL_U, ///< Close back vowel /u/ (F1≈300, F2≈800, F3≈2300, F4≈3500, F5≈4500 Hz)
    };

    //-------------------------------------------------------------------------
    // Per-resonator descriptor
    //-------------------------------------------------------------------------

    /**
     * @struct ResonatorNode
     * @brief State of a single biquad bandpass resonator
     */
    struct ResonatorNode {
        std::shared_ptr<Filters::IIR> filter; ///< Underlying biquad IIR

        double frequency; ///< Centre frequency (Hz)
        double q; ///< Quality factor (dimensionless; higher = narrower bandwidth)
        double gain; ///< Per-resonator output amplitude scale
        double last_output; ///< Most recent process_sample output
        size_t index; ///< Position in network

        std::shared_ptr<Node> exciter; ///< Per-resonator exciter (nullptr = use network-level exciter)
    };

    //-------------------------------------------------------------------------
    // Construction
    //-------------------------------------------------------------------------

    /**
     * @brief Construct a ResonatorNetwork with a formant preset
     * @param num_resonators Number of biquad sections to allocate
     * @param preset Formant frequency/Q configuration to apply at startup
     *
     * Resonators beyond the preset's defined count are initialised at 440 Hz,
     * Q = 10 with unit gain.
     */
    ResonatorNetwork(size_t num_resonators,
        FormantPreset preset = FormantPreset::NONE);

    /**
     * @brief Construct a ResonatorNetwork with explicit frequency and Q vectors
     * @param frequencies Centre frequencies in Hz, one per resonator
     * @param q_values Q factors, one per resonator (must match frequencies.size())
     * @throws std::invalid_argument if frequencies and q_values differ in size
     */
    ResonatorNetwork(const std::vector<double>& frequencies,
        const std::vector<double>& q_values);

    //-------------------------------------------------------------------------
    // NodeNetwork Interface
    //-------------------------------------------------------------------------

    /**
     * @brief Processes num_samples through all resonators and accumulates output
     * @param num_samples Number of audio samples to compute
     *
     * For each sample, each resonator draws from its individual exciter (or
     * the network-level exciter, or zero if none) and processes one sample. All
     * resonator outputs are summed and normalised into m_last_audio_buffer.
     */
    void process_batch(unsigned int num_samples) override;

    /**
     * @brief Returns the number of resonators in the network
     */
    [[nodiscard]] size_t get_node_count() const override { return m_resonators.size(); }

    /**
     * @brief Returns the mixed audio buffer from the last process_batch() call
     */
    [[nodiscard]] std::optional<std::vector<double>> get_audio_buffer() const override;

    /**
     * @brief Returns the last output sample of the resonator at index
     * @param index Resonator index (0-based)
     * @return Last computed output, or nullopt if index is out of range
     */
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;

    //-------------------------------------------------------------------------
    // Parameter Mapping (NodeNetwork overrides)
    //-------------------------------------------------------------------------

    /**
     * @brief Map a scalar node output to a named networkparameter (BROADCAST)
     * @param param_name "frequency", "q", or "gain"
     * @param source Node whose get_last_output() is read each process_batch()
     * @param mode Must be MappingMode::BROADCAST
     */
    void map_parameter(const std::string& param_name,
        const std::shared_ptr<Node>& source,
        MappingMode mode = MappingMode::BROADCAST) override;

    /**
     * @brief Map a NodeNetwork's per-node outputs to a named network parameter (ONE_TO_ONE)
     * @param param_name "frequency", "q", or "gain"
     * @param source_network NodeNetwork with get_node_count() == get_node_count()
     */
    void map_parameter(const std::string& param_name,
        const std::shared_ptr<NodeNetwork>& source_network) override;

    /**
     * @brief Remove a parameter mapping by name
     */
    void unmap_parameter(const std::string& param_name) override;

    //-------------------------------------------------------------------------
    // Excitation
    //-------------------------------------------------------------------------

    /**
     * @brief Set a shared exciter node for all resonators
     * @param exciter Node providing per-sample excitation (e.g., noise, pulse)
     *
     * Per-resonator exciters take priority over this network-level exciter when set.
     */
    void set_exciter(const std::shared_ptr<Node>& exciter);

    /**
     * @brief Clear the network-level exciter
     */
    void clear_exciter();

    /**
     * @brief Set a per-resonator exciter
     * @param index Resonator index (0-based)
     * @param exciter Node providing excitation specifically for this resonator
     * @throws std::out_of_range if index >= get_node_count()
     */
    void set_resonator_exciter(size_t index, const std::shared_ptr<Node>& exciter);

    /**
     * @brief Clear per-resonator exciter, reverting to network-level exciter
     * @param index Resonator index (0-based)
     * @throws std::out_of_range if index >= get_node_count()
     */
    void clear_resonator_exciter(size_t index);

    /**
     * @brief Set a NodeNetwork as a source of per-resonator excitation (ONE_TO_ONE)
     * @param network NodeNetwork with get_node_count() == get_node_count()
     */
    void set_network_exciter(const std::shared_ptr<NodeNetwork>& network);

    /**
     * @brief Clear the network exciter
     */
    void clear_network_exciter();

    //-------------------------------------------------------------------------
    // Per-resonator parameter control
    //-------------------------------------------------------------------------

    /**
     * @brief Set centre frequency of a single resonator and recompute its coefficients
     * @param index Resonator index (0-based)
     * @param frequency New centre frequency in Hz (clamped to [1.0, sample_rate/2 - 1])
     * @throws std::out_of_range if index >= get_node_count()
     */
    void set_frequency(size_t index, double frequency);

    /**
     * @brief Set Q factor of a single resonator and recompute its coefficients
     * @param index Resonator index (0-based)
     * @param q New quality factor (clamped to [0.1, 1000.0])
     * @throws std::out_of_range if index >= get_node_count()
     */
    void set_q(size_t index, double q);

    /**
     * @brief Set amplitude gain of a single resonator
     * @param index Resonator index (0-based)
     * @param gain New linear amplitude scale
     * @throws std::out_of_range if index >= get_node_count()
     */
    void set_resonator_gain(size_t index, double gain);

    //-------------------------------------------------------------------------
    // network-wide control
    //-------------------------------------------------------------------------

    /**
     * @brief Set centre frequency of all resonators uniformly
     * @param frequency New centre frequency in Hz
     */
    void set_all_frequencies(double frequency);

    /**
     * @brief Set Q factor of all resonators uniformly
     * @param q New quality factor
     */
    void set_all_q(double q);

    /**
     * @brief Apply a FormantPreset to the current network
     * @param preset Target vowel/formant configuration
     *
     * Resonators that exceed the preset's defined count retain their current parameters.
     */
    void apply_preset(FormantPreset preset);

    //-------------------------------------------------------------------------
    // Read-only access
    //-------------------------------------------------------------------------

    /**
     * @brief Read-only access to all resonator descriptors
     */
    [[nodiscard]] const std::vector<ResonatorNode>& get_resonators() const { return m_resonators; }

    /**
     * @brief Current audio sample rate
     */
    [[nodiscard]] double get_sample_rate() const { return m_sample_rate; }

    [[nodiscard]] std::optional<std::span<const double>>
    get_node_audio_buffer(size_t index) const override;

    //-------------------------------------------------------------------------
    // Metadata
    //-------------------------------------------------------------------------

    /**
     * @brief Returns network metadata for debugging and visualisation
     *
     * Exposes "num_resonators", "sample_rate", and per-resonator
     * frequency/Q/gain entries keyed as "resonator_N_freq" etc.
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

private:
    //-------------------------------------------------------------------------
    // Internal helpers
    //-------------------------------------------------------------------------

    /**
     * @brief Compute RBJ biquad bandpass coefficients and push them into a resonator's IIR
     * @param r Resonator to update (reads r.frequency, r.q, m_sample_rate)
     */
    void compute_biquad(ResonatorNode& r);

    /**
     * @brief Initialise all resonators from a frequency/Q pair list
     * @param frequencies Centre frequencies in Hz
     * @param qs Q factors
     */
    void build_resonators(const std::vector<double>& frequencies,
        const std::vector<double>& qs);

    /**
     * @brief Translate a FormantPreset into parallel frequency/Q vectors
     * @param preset Requested preset
     * @param n Number of resonators to populate (may be less than preset's defined count)
     * @param out_freqs Output frequency vector
     * @param out_qs Output Q vector
     */
    static void preset_to_vectors(FormantPreset preset,
        size_t n,
        std::vector<double>& out_freqs,
        std::vector<double>& out_qs);

    /**
     * @brief Apply all registered parameter mappings for the current cycle
     */
    void update_mapped_parameters();

    /**
     * @brief Apply a BROADCAST value to the named parameter across all resonators
     * @param param "frequency", "q", or "gain"
     * @param value Scalar value from source node
     */
    void apply_broadcast_parameter(const std::string& param, double value);

    /**
     * @brief Apply ONE_TO_ONE values from a source network to the named parameter
     * @param param "frequency", "q", or "gain"
     * @param source NodeNetwork providing one value per resonator
     */
    void apply_one_to_one_parameter(const std::string& param,
        const std::shared_ptr<NodeNetwork>& source);

    //-------------------------------------------------------------------------
    // Data
    //-------------------------------------------------------------------------

    std::vector<ResonatorNode> m_resonators;

    std::shared_ptr<Node> m_exciter; ///< networ-level shared exciter (may be nullptr)

    std::shared_ptr<NodeNetwork> m_network_exciter; ///< Optional NodeNetwork exciter for ONE_TO_ONE mapping (may be nullptr)

    std::vector<double> m_last_audio_buffer; ///< Mixed output from last process_batch()

    std::vector<std::vector<double>> m_node_buffers; ///< Per-resonator sample buffers populated each process_batch()

    struct ParameterMapping {
        std::string param_name;
        MappingMode mode;
        std::shared_ptr<Node> broadcast_source;
        std::shared_ptr<NodeNetwork> network_source;
    };

    std::vector<ParameterMapping> m_parameter_mappings;
};

} // namespace MayaFlux::Nodes::Network
