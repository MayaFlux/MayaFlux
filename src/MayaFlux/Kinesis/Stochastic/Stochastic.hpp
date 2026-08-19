#pragma once

#include <random>

namespace MayaFlux::Kinesis::Stochastic {

/**
 * @enum Algorithm
 * @brief Stochastic and procedural generation algorithms
 */
enum class Algorithm : uint8_t {
    UNIFORM,
    NORMAL,
    EXPONENTIAL,
    POISSON,
    PERLIN,
    GENDY,
    BROWNIAN,
    PINK,
    BLUE
};

/**
 * @struct GeneratorState
 * @brief Encapsulates internal state for stateful generators
 *
 * Stateful generators (Gendy, Brownian, Perlin) maintain internal state
 * that evolves over successive calls. This structure provides unified
 * state management across different algorithmic approaches.
 *
 * **Future Interaction Pathways:**
 * - External ML inference can populate `internal_buffer` with predicted sequences
 * - Analysis of audio/visual data can derive breakpoints for Gendy synthesis
 * - Cross-domain mappings can modulate `velocity` or `phase` in real-time
 * - Pattern recognition can inject structure into `algorithm_specific` state
 *
 * All fields are publicly accessible for maximum flexibility in live interaction.
 */
struct GeneratorState {
    double current_value { 0.0 };
    double previous_value { 0.0 };
    double velocity { 0.0 };
    double phase { 0.0 };
    uint64_t step_count { 0 };

    std::vector<double> internal_buffer;
    std::map<std::string, std::any> algorithm_specific;

    void reset()
    {
        current_value = 0.0;
        previous_value = 0.0;
        velocity = 0.0;
        phase = 0.0;
        step_count = 0;
        internal_buffer.clear();
        algorithm_specific.clear();
    }
};

/**
 * @class Stochastic
 * @brief Unified generative infrastructure for stochastic and procedural algorithms
 *
 * Provides mathematical primitives for controlled randomness and procedural generation
 * across all computational domains. Unlike traditional random number generators focused
 * on independent samples, Stochastic embraces both memoryless distributions and stateful
 * processes that evolve over time.
 *
 * ## Architectural Philosophy
 * Treats stochastic generation as fundamental mathematical infrastructure rather than
 * domain-specific processing. The same primitives that generate sonic textures can
 * drive visual phenomena, parametric modulation, or data synthesis - the numbers
 * themselves are discipline-agnostic.
 *
 * ## Algorithm Categories
 *
 * **Memoryless Distributions** (each call independent):
 * - UNIFORM: Flat probability across range
 * - NORMAL: Gaussian distribution
 * - EXPONENTIAL: Exponential decay
 * - POISSON: Discrete event distribution
 *
 * **Stateful Processes** (evolution over successive calls):
 * - PERLIN: Coherent gradient noise with spatial/temporal continuity
 * - GENDY: Xenakis dynamic stochastic synthesis (pitch/amplitude breakpoints)
 * - BROWNIAN: Random walk (integrated white noise)
 * - PINK: 1/f noise (equal energy per octave)
 * - BLUE: Rising spectral energy
 *
 * ## Usage Patterns
 *
 * Memoryless generation:
 * ```cpp
 * Stochastic gen(Algorithm::NORMAL);
 * double value = gen(0.0, 1.0);
 * ```
 *
 * Stateful evolution:
 * ```cpp
 * Stochastic walker(Algorithm::BROWNIAN);
 * walker.configure("step_size", 0.01);
 * for (int i = 0; i < 1000; ++i) {
 *     double pos = walker(0.0, 1.0);
 * }
 * ```
 *
 * Multi-dimensional generation:
 * ```cpp
 * Stochastic perlin(Algorithm::PERLIN);
 * double noise_2d = perlin.at(x, y);
 * double noise_3d = perlin.at(x, y, z);
 * ```
 *
 * @note Thread-unsafe for maximum performance. Use separate instances per thread.
 */
class MAYAFLUX_API Stochastic {
public:
    /**
     * @brief Constructs generator with specified algorithm
     * @param algo Algorithm for generation (default: UNIFORM)
     */
    explicit Stochastic(Algorithm algo = Algorithm::UNIFORM);

    /**
     * @brief Seeds entropy source
     * @param seed Seed value for deterministic sequences
     */
    void seed(uint64_t seed);

    /**
     * @brief Changes active algorithm
     * @param algo New generation algorithm
     *
     * Resets internal state when switching algorithms.
     */
    void set_algorithm(Algorithm algo);

    /**
     * @brief Gets current algorithm
     * @return Active algorithm
     */
    [[nodiscard]] inline Algorithm get_algorithm() const { return m_algorithm; }

    /**
     * @brief Configures algorithm-specific parameters
     * @param key Parameter name
     * @param value Parameter value
     *
     * Standard parameters (algorithm-dependent):
     * - NORMAL: "spread" (double) - std deviation divisor
     * - PERLIN: "octaves" (int), "persistence" (double), "frequency" (double)
     * - GENDY: "breakpoints" (int), "amplitude_dist" (Algorithm), "duration_dist" (Algorithm)
     * - BROWNIAN: "step_size" (double), "bounds_mode" (string)
     * - PINK/BLUE: "pole_count" (int)
     *
     * Interactive/Live parameters (future-ready):
     * - GENDY: "breakpoint_amplitudes" (std::vector<double>) - inject external breakpoints
     * - GENDY: "breakpoint_durations" (std::vector<double>) - inject temporal structure
     * - PERLIN: "permutation_table" (std::vector<int>) - custom noise characteristics
     * - ANY: "modulation_source" (std::function<double()>) - external control signal
     * - ANY: "inference_callback" (std::function<void(GeneratorState&)>) - ML-driven state
     *
     * Dynamic reconfiguration is fully supported - call anytime to alter behavior.
     */
    void configure(const std::string& key, std::any value);

    /**
     * @brief Gets configuration parameter
     * @param key Parameter name
     * @return Parameter value or std::nullopt
     */
    [[nodiscard]] std::optional<std::any> get_config(const std::string& key) const;

    /**
     * @brief Generates single value in range
     * @param min Lower bound
     * @param max Upper bound
     * @return Generated value
     *
     * For stateful algorithms, successive calls evolve internal state.
     */
    [[nodiscard]] double operator()(double min, double max);

    /**
     * @brief Multi-dimensional generation (Perlin, spatial noise)
     * @param x Primary coordinate
     * @param y Secondary coordinate (optional)
     * @param z Tertiary coordinate (optional)
     * @return Noise value at coordinates
     */
    [[nodiscard]] double at(double x, double y = 0.0, double z = 0.0);

    /**
     * @brief Batch generation
     * @param min Lower bound
     * @param max Upper bound
     * @param count Number of values
     * @return Vector of generated values
     */
    [[nodiscard]] std::vector<double> batch(double min, double max, size_t count);

    /**
     * @brief Resets internal state for stateful algorithms
     *
     * Memoryless distributions unaffected. Stateful processes return to initial state.
     */
    void reset_state();

    /**
     * @brief Gets current internal state
     * @return Read-only reference to generator state
     *
     * Exposes complete internal state for:
     * - Analysis and visualization of stochastic evolution
     * - Debugging algorithmic behavior
     * - Extracting learned patterns from stateful processes
     * - Cross-domain mapping of generative trajectories
     */
    [[nodiscard]] const GeneratorState& state() const { return m_state; }

    /**
     * @brief Gets mutable internal state
     * @return Mutable reference to generator state
     *
     * Enables direct manipulation of internal state for:
     * - Injecting externally computed breakpoints (Gendy)
     * - Seeding noise fields with analyzed data (Perlin)
     * - Nudging random walks toward attractors (Brownian)
     * - Implementing hybrid human/algorithmic control
     *
     * **Example: Inject ML-inferred Gendy breakpoints**
     * ```cpp
     * auto& state = gen.state_mutable();
     * state.algorithm_specific["breakpoint_amplitudes"] = ml_predicted_amps;
     * state.algorithm_specific["breakpoint_durations"] = ml_predicted_durs;
     * ```
     */
    [[nodiscard]] GeneratorState& state_mutable() { return m_state; }

    // ========================================================================
    // FUTURE INTERACTION INTERFACES (Placeholders for Dynamic Control)
    // ========================================================================
    // These demonstrate intended interaction patterns without implementing them yet.
    // Uncomment and implement as features are needed.

    // /**
    //  * @brief Inject external breakpoints for Gendy synthesis
    //  * @param amplitudes Amplitude control points
    //  * @param durations Duration control points
    //  *
    //  * Enables ML inference, analysis-driven, or hand-crafted breakpoint injection:
    //  * - Analyze audio spectral contour → derive amplitude breakpoints
    //  * - Train ML model on corpus → predict generative trajectories
    //  * - Map visual motion tracking → temporal breakpoint structure
    //  */
    // void inject_gendy_breakpoints(const std::vector<double>& amplitudes,
    //                               const std::vector<double>& durations);

    // /**
    //  * @brief Set external modulation source for any parameter
    //  * @param target_param Parameter to modulate ("phase", "velocity", etc.)
    //  * @param modulator Function returning control value each call
    //  *
    //  * Connects arbitrary external signals to internal generator state:
    //  * ```cpp
    //  * gen.set_modulation("phase", []() { return audio_analysis.get_centroid(); });
    //  * ```
    //  */
    // void set_modulation(const std::string& target_param,
    //                     std::function<double()> modulator);

    // /**
    //  * @brief Register callback for state analysis/visualization
    //  * @param callback Function receiving state snapshot each generation
    //  *
    //  * Enables real-time monitoring and adaptive control:
    //  * ```cpp
    //  * gen.on_generate([](const GeneratorState& s) {
    //  *     visualizer.plot_trajectory(s.current_value, s.velocity);
    //  * });
    //  * ```
    //  */
    // void on_generate(std::function<void(const GeneratorState&)> callback);

    // /**
    //  * @brief Seed Perlin noise with custom permutation table
    //  * @param permutation Custom permutation for noise field
    //  *
    //  * Inject analyzed or designed noise characteristics:
    //  * - Extract permutation from natural textures
    //  * - Design specific correlation properties
    //  * - Create deterministic but complex fields
    //  */
    // void set_perlin_permutation(const std::vector<int>& permutation);

    // /**
    //  * @brief Inject attractor for Brownian motion
    //  * @param attractor_value Target value to drift toward
    //  * @param strength Pull strength (0.0 = no effect, 1.0 = strong)
    //  *
    //  * Creates biased random walks:
    //  * ```cpp
    //  * walker.set_attractor(0.5, 0.1); // Gentle drift toward center
    //  * ```
    //  */
    // void set_attractor(double attractor_value, double strength);

private:
    [[nodiscard]] double generate_memoryless(double min, double max);
    [[nodiscard]] double generate_stateful(double min, double max);
    [[nodiscard]] double generate_perlin_impl(double x, double y, double z);
    [[nodiscard]] double generate_gendy_impl(double min, double max);
    [[nodiscard]] double generate_brownian_impl(double min, double max);
    [[nodiscard]] double generate_colored_noise_impl(double min, double max);

    void validate_range(double min, double max) const;
    void rebuild_distributions_if_needed(double min, double max);

    [[nodiscard]] inline double fast_uniform() noexcept
    {
        m_xorshift_state ^= m_xorshift_state >> 12;
        m_xorshift_state ^= m_xorshift_state << 25;
        m_xorshift_state ^= m_xorshift_state >> 27;
        return static_cast<double>(m_xorshift_state * 0x2545F4914F6CDD1DULL)
            * (1.0 / 18446744073709551616.0);
    }

    std::mt19937 m_engine;
    uint64_t m_xorshift_state;
    Algorithm m_algorithm;

    GeneratorState m_state;
    std::map<std::string, std::any> m_config;

    std::normal_distribution<double> m_normal_dist { 0.0, 1.0 };
    std::exponential_distribution<double> m_exponential_dist { 1.0 };

    double m_cached_min { 0.0 };
    double m_cached_max { 1.0 };
    bool m_dist_dirty { true };
};

/**
 * @brief Creates uniform random generator
 * @return Configured Stochastic instance
 */
inline Stochastic uniform() { return Stochastic(Algorithm::UNIFORM); }

/**
 * @brief Creates Gaussian random generator
 * @param spread Standard deviation divisor (default: 4.0 for ~95% in range)
 * @return Configured Stochastic instance
 */
inline Stochastic gaussian(double spread = 4.0)
{
    Stochastic gen(Algorithm::NORMAL);
    gen.configure("spread", spread);
    return gen;
}

/**
 * @brief Creates Perlin noise generator
 * @param octaves Number of noise octaves (default: 4)
 * @param persistence Amplitude decay per octave (default: 0.5)
 * @return Configured Stochastic instance
 */
inline Stochastic perlin(int octaves = 4, double persistence = 0.5)
{
    Stochastic gen(Algorithm::PERLIN);
    gen.configure("octaves", octaves);
    gen.configure("persistence", persistence);
    return gen;
}

/**
 * @brief Creates Gendy dynamic stochastic generator
 * @param breakpoints Number of control points (default: 12)
 * @return Configured Stochastic instance
 */
inline Stochastic gendy(int breakpoints = 12)
{
    Stochastic gen(Algorithm::GENDY);
    gen.configure("breakpoints", breakpoints);
    return gen;
}

/**
 * @brief Creates Brownian motion generator
 * @param step_size Maximum step per call (default: 0.01)
 * @return Configured Stochastic instance
 */
inline Stochastic brownian(double step_size = 0.01)
{
    Stochastic gen(Algorithm::BROWNIAN);
    gen.configure("step_size", step_size);
    return gen;
}

}
