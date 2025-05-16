#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"

namespace MayaFlux::Buffers {

/**
 * @class LogicProcessor
 * @brief Digital signal processor that applies boolean logic operations to data streams
 *
 * LogicProcessor implements digital logic processing on data streams, enabling
 * complex decision-making, pattern detection, and signal transformation based on
 * boolean logic principles. It bridges Nodes::Generator::Logic nodes with the buffer
 * processing system to enable sophisticated digital signal manipulation.
 *
 * Use cases include:
 * - Binary pattern detection in data streams
 * - Digital control signal generation
 * - Conditional data flow routing
 * - Event triggering based on signal characteristics
 * - Digital state machine implementation
 * - Signal quantization and discretization
 */
class LogicProcessor : public BufferProcessor {
public:
    /**
     * @enum ProcessMode
     * @brief Defines the digital processing strategy for data streams
     *
     * These modes determine how the processor interprets and transforms
     * continuous data into discrete logical states.
     */
    enum class ProcessMode {
        SAMPLE_BY_SAMPLE, ///< Process each sample independently as a discrete event
        THRESHOLD_CROSSING, ///< Only process when binary state changes occur
        EDGE_TRIGGERED, ///< Only process on digital transitions (rising/falling edges)
        STATE_MACHINE ///< Process buffer as a sequence of state transitions
    };

    /**
     * @brief How the logic signal modulates the audio buffer
     */
    enum class ModulationType {
        REPLACE, ///< Replace buffer contents with logic signal
        MULTIPLY, ///< Multiply buffer by logic signal (amplitude modulation)
        ADD, ///< Add logic signal to buffer
        CUSTOM ///< Use custom modulation function
    };

    /**
     * @typedef ModulationFunction
     * @brief Function type for custom digital signal transformations
     *
     * Defines a transformation that combines a logic value (0.0 or 1.0)
     * with an input sample to produce a modulated output sample.
     *
     * Parameters:
     * - double logic_val: The binary logic value (0.0 or 1.0)
     * - double input_val: The original input sample value
     *
     * Returns:
     * - double: The transformed output sample value
     *
     * This type enables implementation of arbitrary digital transformations
     * based on binary logic states, supporting complex conditional processing,
     * digital filtering, and algorithmic decision trees in signal processing.
     */
    using ModulationFunction = std::function<double(double, double)>;

    /**
     * @brief Constructs a LogicProcessor with the specified logic node
     * @param logic The logic node to use for processing
     * @param mode The processing mode to use
     * @param reset_between_buffers Whether to reset logic state between buffer calls
     */
    LogicProcessor(
        std::shared_ptr<Nodes::Generator::Logic> logic,
        ProcessMode mode = ProcessMode::SAMPLE_BY_SAMPLE,
        bool reset_between_buffers = false);

    /**
     * @brief Processes a buffer through the logic node
     * @param buffer The audio buffer to process
     *
     * The processing behavior depends on the selected process mode:
     * - SAMPLE_BY_SAMPLE: Treats each sample as an independent logical input
     * - THRESHOLD_CROSSING: Detects binary state transitions in the data stream
     * - EDGE_TRIGGERED: Responds only to specific transition patterns (rising/falling)
     * - STATE_MACHINE: Processes sequential state transitions with memory of previous states
     *
     * Digital processing use cases:
     * - Binary data extraction from continuous signals
     * - Pulse detection and generation
     * - Hysteresis implementation for noisy signals
     * - Sequential pattern recognition
     */
    void process(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Called when the processor is attached to a buffer
     * @param buffer The buffer this processor is being attached to
     */
    void on_attach(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Called when the processor is detached from a buffer
     * @param buffer The buffer this processor is being detached from
     */
    inline void on_detach(std::shared_ptr<AudioBuffer> buffer) override { }

    /**
     * @brief Generates discrete logic data from input without modifying any buffer
     * @param num_samples Number of samples to generate
     * @param input_data Optional input data to process (uses zeros if empty)
     * @return Whether generation was successful
     *
     * This method allows for offline processing of data through the logic system,
     * useful for analysis, preprocessing, or generating control signals independently
     * of the main signal path.
     */
    bool generate(size_t num_samples, const std::vector<double>& input_data = {});

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
    bool apply(std::shared_ptr<AudioBuffer> buffer,
        ModulationFunction modulation_func = nullptr);

    /**
     * @brief Gets the stored logic data
     * @return Reference to the internal logic data vector
     */
    const std::vector<double>& get_logic_data() const { return m_logic_data; }

    /**
     * @brief Checks if logic data has been generated
     * @return True if logic data is available
     */
    inline bool has_generated_data() const { return m_has_generated_data; }

    /**
     * @brief Sets the processing mode
     * @param mode The new processing mode
     */
    void set_process_mode(ProcessMode mode);

    /**
     * @brief Gets the current processing mode
     * @return The current processing mode
     */
    inline ProcessMode get_process_mode() const { return m_process_mode; }

    /**
     * @brief Set how logic values modulate buffer content
     * @param type Modulation type to use
     *
     * Different modulation types enable various digital transformations:
     * - REPLACE: Binary substitution of values
     * - MULTIPLY: Binary masking/gating
     * - ADD: Digital offset or bias
     * - CUSTOM: Arbitrary digital transformation
     */
    inline void set_modulation_type(ModulationType type) { m_modulation_type = type; }

    /**
     * @brief Get current modulation type
     * @return Current modulation type
     */
    inline ModulationType get_modulation_type() const { return m_modulation_type; }

    /**
     * @brief Set custom modulation function
     *
     * This sets the ModulationType to CUSTOM and uses the provided
     * function for modulating the buffer. The function takes two
     * parameters: the logic value and the audio value, and returns
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
    inline const ModulationFunction& get_modulation_function() const { return m_modulation_function; }

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
    inline bool get_reset_between_buffers() const { return m_reset_between_buffers; }

    /**
     * @brief Sets the threshold for binary state determination
     * @param threshold Threshold value (0.0 to 1.0)
     *
     * Defines the decision boundary for converting continuous values to binary states.
     * Critical for accurate digital interpretation of analog-like signals.
     */
    void set_threshold(double threshold);

    /**
     * @brief Gets the current threshold
     * @return Current threshold value
     */
    inline double get_threshold() const { return m_threshold; }

    /**
     * @brief Sets edge type for edge-triggered mode
     * @param type Type of edge to trigger on
     *
     * Configures which digital transitions (0→1, 1→0, or both) will trigger processing.
     * Essential for pulse detection, event triggering, and state change monitoring.
     */
    void set_edge_type(Nodes::Generator::EdgeType type);

    /**
     * @brief Gets the current edge type
     * @return Current edge type
     */
    inline Nodes::Generator::EdgeType get_edge_type() const { return m_edge_type; }

private:
    std::shared_ptr<Nodes::Generator::Logic> m_logic; ///< Logic node for processing
    ProcessMode m_process_mode; ///< Current processing mode
    bool m_reset_between_buffers; ///< Whether to reset logic between buffers
    double m_threshold; ///< Threshold for binary state determination
    Nodes::Generator::EdgeType m_edge_type; ///< Edge type for transition detection
    double m_last_value; ///< Last processed value (for state tracking)
    bool m_last_state; ///< Last boolean state
    ModulationType m_modulation_type;

    ModulationFunction m_modulation_function; ///< Custom transformation function
    std::vector<double> m_logic_data; ///< Stored logic processing results
    bool m_has_generated_data; ///< Whether logic data has been generated
};

} // namespace MayaFlux::Buffers
