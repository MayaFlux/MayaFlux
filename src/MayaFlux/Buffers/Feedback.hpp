#pragma once

#include "AudioBuffer.hpp"
#include "BufferProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class FeedbackBuffer
 * @brief Specialized buffer implementing computational feedback systems
 *
 * FeedbackBuffer extends StandardAudioBuffer to create a buffer that maintains memory
 * of its previous state, enabling the creation of recursive computational systems.
 * This implementation transcends traditional audio effects, providing a foundation
 * for complex dynamical systems, emergent behaviors, and self-modifying algorithms.
 *
 * Key features:
 * - Implements a discrete-time recursive system with controllable feedback coefficient
 * - Enables creation of complex dynamical systems with memory
 * - Supports emergence of non-linear behaviors through controlled recursion
 * - Provides a foundation for generative algorithms that evolve over time
 *
 * Feedback is a fundamental concept in computational systems that enables complex
 * behaviors to emerge from simple rules. This implementation provides a clean,
 * controlled way to introduce recursive elements without the risks of uncontrolled
 * recursion or stack overflow that can occur in node-based feedback.
 */
class FeedbackBuffer : public StandardAudioBuffer {
public:
    /**
     * @brief Creates a new feedback buffer
     * @param channel_id Channel identifier for this buffer
     * @param num_samples Buffer size in samples
     * @param feedback Feedback coefficient (0.0-1.0)
     *
     * Initializes a buffer that implements a discrete-time recursive system.
     * The feedback parameter controls the coefficient of recursion, determining
     * how strongly the system's past states influence its future evolution.
     */
    FeedbackBuffer(u_int32_t channel_id = 0, u_int32_t num_samples = 512, float feedback = 0.5f);

    /**
     * @brief Sets the feedback coefficient
     * @param amount Feedback coefficient (0.0-1.0)
     *
     * Controls the strength of recursion in the system:
     * - 0.0: No recursion (behaves like a standard buffer)
     * - 0.5: Balanced influence between new input and previous state
     * - 1.0: Maximum recursion (can lead to saturation or chaotic behavior)
     */
    inline void set_feedback(float amount) { m_feedback_amount = amount; }

    /**
     * @brief Gets the current feedback coefficient
     * @return Current feedback coefficient (0.0-1.0)
     */
    inline float get_feedback() const { return m_feedback_amount; }

    /**
     * @brief Gets mutable access to the previous state vector
     * @return Reference to the vector containing previous state data
     *
     * This provides direct access to the system's previous state, which can be
     * useful for advanced algorithms or analysis. Use with caution as modifying
     * this directly can affect the system's evolution.
     */
    inline std::vector<double>& get_previous_buffer() { return m_previous_buffer; }

    /**
     * @brief Gets read-only access to the previous state vector
     * @return Const reference to the vector containing previous state data
     */
    inline const std::vector<double>& get_previous_buffer() const { return m_previous_buffer; }

    /**
     * @brief Processes this buffer using its default processor
     *
     * For a FeedbackBuffer, this involves applying the recursive algorithm
     * that combines current input with the previous state according to
     * the feedback coefficient.
     */
    void process_default() override;

protected:
    /**
     * @brief Creates the default processor for this buffer type
     * @return Shared pointer to a FeedbackProcessor
     *
     * FeedbackBuffers use a FeedbackProcessor as their default processor,
     * which implements the recursive algorithm.
     */
    std::shared_ptr<BufferProcessor> create_default_processor() override;

    /**
     * @brief Default processor for this buffer
     *
     * This is a FeedbackProcessor configured with the buffer's feedback coefficient.
     */
    std::shared_ptr<BufferProcessor> m_default_processor;

private:
    /**
     * @brief Feedback coefficient (0.0-1.0)
     *
     * Controls the strength of recursion in the system.
     */
    float m_feedback_amount;

    /**
     * @brief Storage for the previous system state
     *
     * This vector maintains a copy of the system's state from the previous
     * processing cycle, enabling the implementation of recursive algorithms.
     */
    std::vector<double> m_previous_buffer;
};

/**
 * @class FeedbackProcessor
 * @brief Processor that implements recursive computational algorithms
 *
 * FeedbackProcessor is a specialized buffer processor that implements discrete-time
 * recursive algorithms by combining a system's current state with its previous state.
 * It can be applied to any AudioBuffer, not just FeedbackBuffer, allowing recursive
 * properties to be added to existing computational pipelines.
 *
 * Unlike stateless processors, FeedbackProcessor maintains memory between processing
 * cycles, storing the previous system state for use in the next cycle. This
 * memory-based behavior enables the emergence of complex temporal patterns and
 * evolutionary behaviors.
 *
 * Applications:
 * - Generative algorithms with memory and evolution
 * - Simulation of complex dynamical systems
 * - Creation of emergent, self-modifying behaviors
 * - Implementation of recursive mathematical functions
 * - Cross-domain feedback systems (audio influencing visual, data influencing audio, etc.)
 */
class FeedbackProcessor : public BufferProcessor {

public:
    /**
     * @brief Creates a new feedback processor
     * @param feedback Feedback coefficient (0.0-1.0)
     *
     * Initializes a processor that implements a recursive algorithm
     * combining a system's current state with its previous state
     * according to the specified feedback coefficient.
     */
    FeedbackProcessor(float feedback = 0.5f);

    /**
     * @brief Processes a buffer by applying the recursive algorithm
     * @param buffer Buffer to process
     *
     * This method:
     * 1. Combines the current state with the stored previous state
     * 2. Stores the resulting output as the new previous state
     *
     * The combination is weighted by the feedback coefficient, with higher
     * values resulting in stronger influence from the previous state.
     */
    void process(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Called when this processor is attached to a buffer
     * @param buffer Buffer this processor is being attached to
     *
     * Initializes the previous state storage to match the size of the
     * attached buffer. If the buffer is a FeedbackBuffer, the processor
     * will use its internal previous state.
     */
    void on_attach(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Called when this processor is detached from a buffer
     * @param buffer Buffer this processor is being detached from
     *
     * Cleans up any buffer-specific state.
     */
    void on_detach(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Sets the feedback coefficient
     * @param amount Feedback coefficient (0.0-1.0)
     */
    inline void set_feedback(float amount) { m_feedback_amount = amount; }

    /**
     * @brief Gets the current feedback coefficient
     * @return Current feedback coefficient (0.0-1.0)
     */
    inline float get_feedback() const { return m_feedback_amount; }

private:
    /**
     * @brief Feedback coefficient (0.0-1.0)
     */
    float m_feedback_amount;

    /**
     * @brief Storage for the previous system state
     *
     * This vector maintains a copy of the system's state from the previous
     * processing cycle, enabling the implementation of recursive algorithms.
     */
    std::vector<double> m_previous_buffer;

    /**
     * @brief Flag indicating whether to use the buffer's internal previous state
     *
     * If the attached buffer is a FeedbackBuffer, the processor will use its
     * internal previous state instead of maintaining its own.
     */
    bool m_using_internal_buffer;
};
}
