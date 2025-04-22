#pragma once

#include "AudioBuffer.hpp"
#include "BufferProcessor.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

/**
 * @class NodeSourceProcessor
 * @brief Processor that bridges computational nodes and data buffers
 *
 * NodeSourceProcessor serves as a data flow connector between the node computation system
 * and the buffer storage system, enabling the capture and persistence of dynamically
 * generated values. This component is fundamental for integrating real-time, sample-by-sample
 * computational processes with block-based data storage and transformation.
 *
 * Key capabilities:
 * - Captures sequential output from computational nodes into structured data buffers
 * - Provides configurable interpolation between existing and incoming data streams
 * - Supports both accumulative and replacement data flow patterns
 *
 * This processor enables powerful computational patterns such as:
 * - Capturing generative algorithm outputs for analysis or visualization
 * - Creating persistent records of ephemeral computational processes
 * - Implementing hybrid computational models that combine continuous and discrete processing
 * - Building feedback loops between different computational domains
 */
class NodeSourceProcessor : public BufferProcessor {
public:
    /**
     * @brief Creates a new processor that connects a computational node to data buffers
     * @param node Source node that generates sequential data values
     * @param mix Interpolation coefficient between existing and incoming data (0.0-1.0)
     * @param clear_before_process Whether to reset the buffer before adding node output
     *
     * The mix parameter controls the interpolation between existing and incoming data:
     * - 0.0: Preserve existing data (incoming values ignored)
     * - 0.5: Equal interpolation between existing and incoming values
     * - 1.0: Replace with incoming values (existing data overwritten)
     */
    NodeSourceProcessor(std::shared_ptr<Nodes::Node> node, float mix = 0.5f, bool clear_before_process = false);

    /**
     * @brief Captures node computation output into a buffer
     * @param buffer Buffer to store the processed data
     *
     * This method implements a configurable data flow pattern:
     * 1. Optionally resets the buffer if clear_before_process is true
     * 2. Generates sequential values from the computational node
     * 3. Applies the specified interpolation between existing and incoming data
     */
    void process(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Sets the interpolation coefficient between existing and incoming data
     * @param mix Interpolation coefficient (0.0-1.0)
     */
    inline void set_mix(float mix) { m_mix = mix; }

    /**
     * @brief Gets the current interpolation coefficient
     * @return Current interpolation coefficient (0.0-1.0)
     */
    inline float get_mix() const { return m_mix; }

private:
    /**
     * @brief Source node that generates sequential data values
     */
    std::shared_ptr<Nodes::Node> m_node;

    /**
     * @brief Interpolation coefficient between existing and incoming data (0.0-1.0)
     */
    float m_mix;

    /**
     * @brief Whether to reset the buffer before adding node output
     */
    bool m_clear_before_process;
};

/**
 * @class NodeBuffer
 * @brief Specialized buffer that automatically captures output from computational nodes
 *
 * NodeBuffer extends StandardAudioBuffer to create a buffer with an intrinsic connection
 * to a computational node. It automatically captures and persists the node's sequential
 * output, creating a bridge between ephemeral computation and persistent data storage.
 *
 * This class implements a composite pattern, combining a data buffer with a NodeSourceProcessor
 * to create a self-contained component for capturing computational outputs. This simplifies
 * the creation of data persistence mechanisms within computational networks.
 *
 * Applications:
 * - Creating persistent records of generative algorithm outputs
 * - Implementing time-delayed computational feedback systems
 * - Building data bridges between different computational domains
 * - Enabling analysis and visualization of dynamic computational processes
 */
class NodeBuffer : public StandardAudioBuffer {

public:
    /**
     * @brief Creates a new buffer connected to a computational node
     * @param channel_id Channel identifier for this buffer
     * @param num_samples Buffer size in samples
     * @param source Source node that generates sequential data values
     * @param clear_before_process Whether to reset the buffer before adding node output
     *
     * Initializes a buffer that automatically captures output from the specified computational
     * node when processed. The buffer is configured with a NodeSourceProcessor as its
     * default processor, creating a self-contained data capture system.
     */
    NodeBuffer(u_int32_t channel_id, u_int32_t num_samples, std::shared_ptr<Nodes::Node> source, bool clear_before_process = false);

    /**
     * @brief Sets whether to reset the buffer before processing node output
     * @param value true to reset before processing, false to interpolate with existing content
     */
    inline void set_clear_before_process(bool value) { m_clear_before_process = value; }

    /**
     * @brief Gets whether the buffer is reset before processing node output
     * @return true if the buffer is reset before processing, false otherwise
     */
    inline bool get_clear_before_process() const { return m_clear_before_process; }

    /**
     * @brief Processes this buffer using its default processor
     *
     * For a NodeBuffer, this involves capturing sequential output from the source node
     * and storing it in the buffer according to the configured interpolation coefficient
     * and clear_before_process setting.
     */
    void process_default() override;

protected:
    /**
     * @brief Creates the default processor for this buffer type
     * @return Shared pointer to a NodeSourceProcessor
     *
     * NodeBuffers use a NodeSourceProcessor as their default processor,
     * which handles capturing output from the source node.
     */
    std::shared_ptr<BufferProcessor> create_default_processor() override;

    /**
     * @brief Default processor for this buffer
     *
     * This is a NodeSourceProcessor configured to capture output
     * from the source node.
     */
    std::shared_ptr<BufferProcessor> m_default_processor;

private:
    /**
     * @brief Source node that generates sequential data values
     */
    std::shared_ptr<Nodes::Node> m_source_node;

    /**
     * @brief Whether to reset the buffer before adding node output
     */
    bool m_clear_before_process;
};
}
