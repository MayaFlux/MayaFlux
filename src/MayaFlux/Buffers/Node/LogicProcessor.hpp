#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;

/**
 * @class LogicProcessor
 * @brief Digital signal processor that applies boolean logic operations to data streams
 *
 * LogicProcessor bridges Nodes::Generator::Logic nodes with the buffer processing system,
 * enabling sophisticated digital signal manipulation through various modulation strategies.
 *
 * The processor's job is simple:
 * 1. Iterate through buffer samples
 * 2. Generate logic values (0.0 or 1.0) by calling the Logic node
 * 3. Apply logic to buffer data using a modulation strategy
 *
 * All logic computation (threshold detection, edge detection, state machines, etc.)
 * is handled by the Logic node itself. The processor only manages iteration and
 * application of results.
 *
 * Use cases include:
 * - Binary pattern detection in data streams
 * - Digital control signal generation
 * - Conditional data flow routing
 * - Event triggering based on signal characteristics
 * - Digital state machine implementation
 * - Signal quantization and discretization
 */
class MAYAFLUX_API LogicProcessor : public BufferProcessor {
public:
    /**
     * @enum ModulationType
     * @brief Defines how logic values modulate buffer content
     *
     * These are readymade strategies for applying binary logic (0.0/1.0) to
     * continuous audio data, providing common compositional primitives for
     * logic-based signal processing.
     */
    enum class ModulationType : uint8_t {
        REPLACE, ///< Replace buffer with logic values: out = logic
        MULTIPLY, ///< Gate/mask buffer: out = logic * buffer (standard audio gate)
        ADD, ///< Offset buffer: out = logic + buffer

        INVERT_ON_TRUE, ///< Invert signal when logic is true: out = logic ? -buffer : buffer
        HOLD_ON_FALSE, ///< Hold last value when logic is false: out = logic ? buffer : last_value
        ZERO_ON_FALSE, ///< Silence when logic is false: out = logic ? buffer : 0.0
        CROSSFADE, ///< Smooth interpolation: out = lerp(0.0, buffer, logic)
        THRESHOLD_REMAP, ///< Binary value selection: out = logic ? high_val : low_val
        SAMPLE_AND_HOLD, ///< Sample on logic change: out = logic_changed ? buffer : held_value

        CUSTOM ///< User-defined modulation function
    };

    /**
     * @typedef ModulationFunction
     * @brief Function type for custom digital signal transformations
     *
     * Defines a transformation that combines a logic value (0.0 or 1.0)
     * with a buffer sample to produce a modulated output sample.
     *
     * Parameters:
     * - double logic_val: The binary logic value (0.0 or 1.0)
     * - double buffer_val: The original buffer sample value
     *
     * Returns:
     * - double: The transformed output sample value
     *
     * This enables implementation of arbitrary digital transformations
     * based on binary logic states, supporting complex conditional processing,
     * digital filtering, and algorithmic decision trees in signal processing.
     */
    using ModulationFunction = std::function<double(double, double)>;

    /**
     * @brief Constructs a LogicProcessor with internal Logic node
     * @param args Arguments to pass to the Logic constructor
     *
     * Creates a LogicProcessor that constructs its own Logic node internally.
     */
    template <typename... Args>
        requires std::constructible_from<Nodes::Generator::Logic, Args...>
    LogicProcessor(Args&&... args)
        : m_logic(std::make_shared<Nodes::Generator::Logic>(std::forward<Args>(args)...))
        , m_reset_between_buffers(false)
        , m_use_internal(true)
        , m_modulation_type(ModulationType::REPLACE)
        , m_has_generated_data(false)
        , m_high_value(1.0)
        , m_low_value(0.0)
        , m_last_held_value(0.0)
        , m_last_logic_value(0.0)
    {
    }

    /**
     * @brief Constructs a LogicProcessor with external Logic node
     * @param logic The logic node to use for processing
     * @param reset_between_buffers Whether to reset logic state between buffer calls
     *
     * Creates a LogicProcessor that uses an external Logic node.
     * NOTE: Using external Logic node implies side effects of any processing chain
     * the node is connected to.
     */
    LogicProcessor(
        const std::shared_ptr<Nodes::Generator::Logic>& logic,
        bool reset_between_buffers = false);

    /**
     * @brief Processes a buffer through the logic node
     * @param buffer The audio buffer to process
     *
     * Processing steps:
     * 1. Iterates through each sample in the buffer
     * 2. Calls Logic node's process_sample() for each sample to get logic value
     * 3. Applies the selected modulation strategy to combine logic with buffer data
     *
     * The Logic node handles all temporal state, callbacks, and logic computation.
     * The processor only manages iteration and modulation application.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Called when the processor is attached to a buffer
     * @param buffer The buffer this processor is being attached to
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Called when the processor is detached from a buffer
     * @param buffer The buffer this processor is being detached from
     */
    inline void on_detach(const std::shared_ptr<Buffer>&) override { }

    /**
     * @brief Generates discrete logic data from input without modifying any buffer
     * @param num_samples Number of samples to generate
     * @param input_data Input data to process through logic node
     * @return Whether generation was successful
     *
     * This method allows for offline processing of data through the logic system,
     * useful for analysis, preprocessing, or generating control signals independently
     * of the main signal path.
     */
    bool generate(size_t num_samples, const std::vector<double>& input_data);

    /**
     * @brief Applies stored logic data to the given buffer
     * @param buffer Buffer to apply logic data to
     * @param modulation_func Optional function defining how to apply logic to audio
     * @return Whether application was successful
     *
     * Use cases:
     * - Conditional data transformation
     * - Digital gating of signals
     * - Binary masking operations
     * - Custom digital signal processing chains
     */
    bool apply(const std::shared_ptr<Buffer>& buffer,
        ModulationFunction modulation_func = nullptr);

    /**
     * @brief Gets the stored logic data
     * @return Reference to the internal logic data vector
     */
    [[nodiscard]] const std::vector<double>& get_logic_data() const { return m_logic_data; }

    /**
     * @brief Checks if logic data has been generated
     * @return True if logic data is available
     */
    [[nodiscard]] inline bool has_generated_data() const { return m_has_generated_data; }

    /**
     * @brief An independent processor over a clone of the node
     *
     * Same reset policy, modulation type, custom modulation function and
     * remap values; the node is cloned, so its history and coefficient array
     * are not shared. Attach one clone per channel instead of one processor to
     * several.
     */
    [[nodiscard]] std::shared_ptr<LogicProcessor> clone() const;

    /**
     * @brief Reads another buffer as an extra input slot of a parallel node
     * @param buffer The key, for example another channel or a sidechain
     *
     * With a parallel Logic node, every sample is evaluated with one slot per
     * key, in the order added, followed by this buffer's own sample, the same
     * order input nodes use. The key is read as it stands when this buffer is
     * processed, so process it first; a key shorter than this buffer or no
     * longer alive reads as silence. Keys are ignored by a node that is not
     * parallel.
     */
    void add_key(const std::shared_ptr<AudioBuffer>& buffer);

    /**
     * @brief Removes every key
     */
    void clear_keys();

    /**
     * @brief Processes every sample as a word of bit streams, one node per bit
     * @param bits Word width, 1 to 24; 0 turns it off
     *
     * Each sample becomes an offset binary word (Kinesis::Discrete::to_word) and
     * every bit position runs through its own clone of the node, so each plane
     * keeps its own state. Planes are presented to the node as 0.0 or 1.0 and
     * the output bits are recombined into the sample, so the logic data is a
     * value in [-1, 1] rather than a bit. With a parallel node and keys, plane j
     * sees plane j of every key and of this buffer: bitwise word operations
     * across channels. The clones are taken at the next block and again when
     * the node is replaced or this is called.
     */
    void set_bit_planes(uint32_t bits);

    /**
     * @brief Treats the samples of a block as cells of an automaton
     * @param radius Cells on each side that feed a cell
     * @param generations Steps per block; 0 turns it off
     * @param wrap Whether the block's ends meet, otherwise cells beyond them are 0
     * @param memory Whether the block perturbs the previous result instead of replacing it
     *
     * Needs a parallel node. A cell is 1 when its sample is above zero, and the
     * node is evaluated for every cell with 2 * radius + 1 slots, slot 0 the
     * cell furthest to the right and the last slot the furthest to the left,
     * so Wolfram's rule numbers work with `input_truth_table_state(3, rule)`.
     * With memory the block's cells are XORed into the previous block's final
     * generation, so the pattern evolves across blocks; use one processor per
     * channel (clone) for that. The logic data is the final generation, 0 or 1.
     */
    void set_neighbourhood(size_t radius, size_t generations = 1, bool wrap = true, bool memory = false);

    /**
     * @brief Turns the automaton off
     */
    void clear_neighbourhood();

    /**
     * @brief Set how logic values modulate buffer content
     * @param type Modulation type to use
     *
     * Different modulation types enable various digital transformations:
     * - REPLACE: Binary substitution of values
     * - MULTIPLY: Binary masking/gating
     * - ADD: Digital offset or bias
     * - INVERT_ON_TRUE: Phase inversion based on logic state
     * - HOLD_ON_FALSE: Sample-and-hold behavior
     * - ZERO_ON_FALSE: Hard gating (silence on false)
     * - CROSSFADE: Smooth amplitude interpolation
     * - THRESHOLD_REMAP: Binary value mapping
     * - SAMPLE_AND_HOLD: Update only on logic changes
     * - CUSTOM: Arbitrary digital transformation
     */
    inline void set_modulation_type(ModulationType type) { m_modulation_type = type; }

    /**
     * @brief Get current modulation type
     * @return Current modulation type
     */
    [[nodiscard]] inline ModulationType get_modulation_type() const { return m_modulation_type; }

    /**
     * @brief Set custom modulation function
     *
     * This sets the ModulationType to CUSTOM and uses the provided
     * function for modulating the buffer. The function takes two
     * parameters: the logic value and the buffer value, and returns
     * the modulated result.
     *
     * Use cases:
     * - Conditional data transformation based on logic state
     * - Complex digital signal processing operations
     * - Custom digital filtering based on binary conditions
     * - Algorithmic decision trees in signal processing
     *
     * @param func Custom modulation function
     */
    void set_modulation_function(ModulationFunction func);

    /**
     * @brief Get current modulation function
     * @return Current modulation function
     */
    [[nodiscard]] inline const ModulationFunction& get_modulation_function() const { return m_modulation_function; }

    /**
     * @brief Sets high and low values for THRESHOLD_REMAP mode
     * @param high_val Value to use when logic is true (1.0)
     * @param low_val Value to use when logic is false (0.0)
     */
    inline void set_threshold_remap_values(double high_val, double low_val)
    {
        m_high_value = high_val;
        m_low_value = low_val;
    }

    /**
     * @brief Gets the high value for THRESHOLD_REMAP mode
     * @return High value
     */
    [[nodiscard]] inline double get_high_value() const { return m_high_value; }

    /**
     * @brief Gets the low value for THRESHOLD_REMAP mode
     * @return Low value
     */
    [[nodiscard]] inline double get_low_value() const { return m_low_value; }

    /**
     * @brief Sets whether to reset logic state between buffer calls
     * @param reset True to reset between buffers, false otherwise
     *
     * Controls whether the processor maintains state memory across buffer boundaries,
     * enabling either stateless processing or continuous state tracking.
     */
    inline void set_reset_between_buffers(bool reset) { m_reset_between_buffers = reset; }

    /**
     * @brief Gets whether logic state is reset between buffer calls
     * @return True if logic state is reset between buffers
     */
    [[nodiscard]] inline bool get_reset_between_buffers() const { return m_reset_between_buffers; }

    /**
     * @brief Checks if the processor is using the internal logic node
     * @return True if using internal logic node, false otherwise
     */
    [[nodiscard]] inline bool is_using_internal() const { return m_use_internal; }

    /**
     * @brief Forces the processor to use a new internal logic node
     *
     * This replaces the current logic node with a new one constructed from the
     * provided arguments, ensuring the processor uses its own internal logic
     * instead of an external one.
     */
    template <typename... Args>
        requires std::constructible_from<Nodes::Generator::Logic, Args...>
    void force_use_internal(Args&&... args)
    {
        m_pending_logic = std::make_shared<Nodes::Generator::Logic>(std::forward<Args>(args)...);
    }

    /**
     * @brief Updates the logic node used for processing
     * @param logic New logic node to use
     *
     * NOTE: Using external Logic node implies side effects of any processing chain
     * the node is connected to. This could mean that the buffer data is not used as
     * input when the node's cached value is used.
     */
    inline void update_logic_node(const std::shared_ptr<Nodes::Generator::Logic>& logic)
    {
        m_pending_logic = logic;
    }

    /**
     * @brief Gets the logic node used for processing
     * @return Logic node used for processing
     */
    [[nodiscard]] inline std::shared_ptr<Nodes::Generator::Logic> get_logic() const { return m_logic; }

private:
    std::shared_ptr<Nodes::Generator::Logic> m_logic; ///< Logic node for processing
    bool m_reset_between_buffers; ///< Whether to reset logic between buffers
    bool m_use_internal; ///< Whether to use internal logic node
    ModulationType m_modulation_type; ///< How logic values modulate buffer content
    std::shared_ptr<Nodes::Generator::Logic> m_pending_logic; ///< Pending logic node update

    ModulationFunction m_modulation_function; ///< Custom transformation function
    std::vector<double> m_logic_data; ///< Stored logic processing results
    bool m_has_generated_data; ///< Whether logic data has been generated

    // State for special modulation modes
    double m_high_value; ///< High value for THRESHOLD_REMAP
    double m_low_value; ///< Low value for THRESHOLD_REMAP
    double m_last_held_value; ///< Last held value for HOLD_ON_FALSE and SAMPLE_AND_HOLD
    double m_last_logic_value; ///< Previous logic value for change detection

    std::vector<std::weak_ptr<AudioBuffer>> m_keys; ///< Buffers read as extra input slots
    uint32_t m_plane_bits {}; ///< Word width when processing bit planes, 0 when off
    size_t m_radius {}; ///< Neighbourhood radius of the automaton
    size_t m_generations {}; ///< Automaton steps per block, 0 when off
    bool m_wrap { true }; ///< Whether the automaton's block ends meet
    bool m_memory {}; ///< Whether blocks perturb the previous automaton result
    std::vector<std::shared_ptr<Nodes::Generator::Logic>> m_plane_nodes; ///< One clone of the node per bit plane
    std::shared_ptr<Nodes::Generator::Logic> m_plane_template; ///< The node the plane clones were taken from
    std::vector<std::shared_ptr<AudioBuffer>> m_live_keys; ///< Keys locked for the current block
    std::vector<uint32_t> m_key_words; ///< Words of the keys at the current sample
    std::vector<double> m_slots; ///< Input slots handed to the node
    std::vector<uint8_t> m_cells; ///< Automaton cells
    std::vector<uint8_t> m_next_cells; ///< Automaton cells of the next generation
    std::vector<uint8_t> m_previous_cells; ///< Last automaton result, kept when memory is on

    void generate_temporal(size_t num_samples, const std::vector<double>& input_data);
    void generate_keyed(size_t num_samples, const std::vector<double>& input_data);
    void generate_planes(size_t num_samples, const std::vector<double>& input_data);
    void generate_spatial(size_t num_samples, const std::vector<double>& input_data);
    void lock_keys();
};

} // namespace MayaFlux::Buffers
