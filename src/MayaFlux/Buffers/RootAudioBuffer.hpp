#pragma once

#include "AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class RootAudioBuffer
 * @brief Top-level aggregation buffer for computational data streams
 *
 * RootAudioBuffer serves as the final convergence point for data streams in each channel
 * before output to hardware interfaces. Similar to RootNode in the node system,
 * there is typically one RootAudioBuffer per output channel in a multi-channel system.
 *
 * Key responsibilities:
 * - Aggregating and combining data from multiple tributary buffers
 * - Receiving direct output from computational node networks (via set_node_output)
 * - Applying final normalization and boundary enforcement to ensure valid output
 * - Ensuring thread-safe access to shared data resources
 *
 * RootAudioBuffer implements a hierarchical data aggregation pattern where multiple
 * computational streams (child buffers and node output) are combined through a
 * configurable mixing algorithm before being transmitted to hardware interfaces.
 */
class RootAudioBuffer : public StandardAudioBuffer {
public:
    /**
     * @brief Creates a new root aggregation buffer for a channel
     * @param channel_id Channel identifier in multi-channel systems
     * @param num_samples Buffer capacity in samples (default: 512)
     *
     * Initializes a root buffer with the specified channel ID and capacity.
     * The buffer is automatically configured with a ChannelProcessor as
     * its default processor for data aggregation.
     */
    RootAudioBuffer(u_int32_t channel_id, u_int32_t num_samples = 512);

    /**
     * @brief Virtual destructor for proper resource management
     */
    ~RootAudioBuffer() override = default;

    /**
     * @brief Adds a tributary buffer to this root buffer
     * @param buffer Child buffer to add to the aggregation hierarchy
     *
     * Tributary buffers contribute their data to the root buffer
     * when the root buffer is processed. This allows multiple computational
     * streams to be combined into a single output channel.
     */
    void add_child_buffer(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Removes a tributary buffer from this root buffer
     * @param buffer Child buffer to remove from the aggregation hierarchy
     */
    void remove_child_buffer(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Gets all tributary buffers in the aggregation hierarchy
     * @return Constant reference to the vector of tributary buffers
     */
    const std::vector<std::shared_ptr<AudioBuffer>>& get_child_buffers() const;

    /**
     * @brief Processes this buffer using its default aggregation processor
     *
     * For a root buffer, this typically involves:
     * 1. Processing all tributary buffers to ensure current data
     * 2. Combining tributary outputs with direct node network output
     * 3. Applying final normalization to ensure valid output ranges
     *
     * This method is thread-safe and can be called from real-time threads.
     */
    virtual void process_default() override;

    /**
     * @brief Resets all data values in this buffer and its tributaries
     *
     * Initializes all sample values to zero in this buffer and optionally
     * in all tributary buffers in the aggregation hierarchy.
     */
    virtual void clear() override;

    /**
     * @brief Resizes this buffer and all tributary buffers
     * @param num_samples New buffer capacity in samples
     *
     * Adjusts the capacity of this buffer and all its tributary buffers to
     * ensure consistent buffer dimensions throughout the aggregation hierarchy.
     */
    virtual void resize(u_int32_t num_samples) override;

    /**
     * @brief Sets direct node network output data for this buffer
     * @param data Vector of data samples from node network processing
     *
     * This allows computational node networks to directly contribute data
     * to the root buffer, which is combined with tributary buffer outputs.
     * The data is copied to ensure thread safety between computational domains.
     */
    void set_node_output(const std::vector<double>& data);

    /**
     * @brief Gets the current node network output data
     * @return Constant reference to the node output vector
     */
    inline const std::vector<double>& get_node_output() const { return m_node_output; }

    /**
     * @brief Checks if this buffer has node network output data
     * @return true if node output is present, false otherwise
     */
    inline bool has_node_output() const { return m_has_node_output; }

    /**
     * @brief Gets the number of tributary buffers in the aggregation hierarchy
     * @return Number of tributary buffers
     */
    inline size_t get_num_children() const { return m_child_buffers.size(); }

protected:
    /**
     * @brief Vector of tributary buffers that contribute to this root buffer
     */
    std::vector<std::shared_ptr<AudioBuffer>> m_child_buffers;

    /**
     * @brief Creates the default processor for this buffer type
     * @return Shared pointer to a ChannelProcessor
     *
     * Root buffers use a ChannelProcessor as their default processor,
     * which handles combining tributary buffers and node network output.
     */
    virtual std::shared_ptr<BufferProcessor> create_default_processor() override;

private:
    /**
     * @brief Data received directly from computational node networks
     */
    std::vector<double> m_node_output;

    /**
     * @brief Flag indicating if node network output data is present
     */
    bool m_has_node_output;

    /**
     * @brief Mutex for thread-safe access to shared data resources
     *
     * Protects access to the buffer data during processing and
     * when setting node output, ensuring thread safety between
     * real-time processing threads and other computational threads.
     */
    std::mutex m_mutex;
};

/**
 * @class ChannelProcessor
 * @brief Processor that implements hierarchical data aggregation for root buffers
 *
 * ChannelProcessor is the default processor for RootAudioBuffer objects.
 * It implements a configurable algorithm for combining data from tributary
 * buffers and direct node network output to produce the final output for a channel.
 *
 * This processor is automatically created and attached to root buffers,
 * but can be replaced with custom aggregation algorithms if needed.
 */
class ChannelProcessor : public BufferProcessor {
public:
    /**
     * @brief Creates a new channel aggregation processor
     * @param root_buffer Pointer to the root buffer this processor will manage
     *
     * The processor maintains a raw pointer to its root buffer to avoid
     * circular references, as the root buffer already owns a shared_ptr
     * to this processor in the object composition hierarchy.
     */
    ChannelProcessor(RootAudioBuffer* root_buffer);

    /**
     * @brief Processes a buffer by combining tributary buffers and node network output
     * @param buffer Buffer to process (should be the root buffer)
     *
     * This method implements a hierarchical data aggregation algorithm:
     * 1. Processes all tributary buffers to ensure current data
     * 2. Combines their outputs into the root buffer using a weighted averaging algorithm
     * 3. Incorporates node network output if present
     *
     * The combination algorithm can be customized in the implementation.
     */
    void process(std::shared_ptr<AudioBuffer> buffer) override;

private:
    /**
     * @brief Pointer to the root buffer this processor manages
     *
     * This is a raw pointer to avoid circular references in the object
     * composition hierarchy, as the root buffer already owns a shared_ptr
     * to this processor.
     */
    RootAudioBuffer* m_root_buffer;
};

/**
 * @class FinalLimiterProcessor
 * @brief Processor that enforces boundary conditions on output data
 *
 * FinalLimiterProcessor is typically used as the final processor in
 * a root buffer's processing chain. It ensures that output values
 * remain within defined boundaries before transmission to hardware interfaces.
 *
 * This boundary enforcement is critical for root buffers since they connect
 * directly to hardware interfaces, where out-of-range values can cause
 * distortion, artifacts, or potentially damage physical components.
 */
class FinalLimiterProcessor : public Buffers::BufferProcessor {
public:
    /**
     * @brief Processes a buffer by enforcing boundary conditions
     * @param buffer Buffer to process
     *
     * This method applies a non-linear boundary enforcement algorithm to ensure
     * all values stay within the valid range (typically -1.0 to 1.0) before
     * being transmitted to hardware interfaces, while preserving the perceptual
     * characteristics of the original signal.
     */
    void process(std::shared_ptr<Buffers::AudioBuffer> buffer) override;
};

}
