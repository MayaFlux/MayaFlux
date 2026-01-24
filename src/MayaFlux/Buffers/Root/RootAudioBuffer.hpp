#pragma once

#include "RootBuffer.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
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
class MAYAFLUX_API RootAudioBuffer : public RootBuffer<AudioBuffer> {
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
    RootAudioBuffer(uint32_t channel_id, uint32_t num_samples = 512);

    void initialize();

    /**
     * @brief Virtual destructor for proper resource management
     */
    ~RootAudioBuffer() override = default;

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
    void process_default() override;

    /**
     * @brief Resizes this buffer and all tributary buffers
     * @param num_samples New buffer capacity in samples
     *
     * Adjusts the capacity of this buffer and all its tributary buffers to
     * ensure consistent buffer dimensions throughout the aggregation hierarchy.
     */
    void resize(uint32_t num_samples) override;

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
     * @brief Activates/deactivates processing for the current token
     * @param active Whether this buffer should process when its token is active
     *
     * For RootAudioBuffer, this controls whether the buffer participates in
     * token-based processing cycles. When inactive, the buffer won't process
     * even if its token is being processed by the system.
     */
    inline void set_token_active(bool active) override { m_token_active = active; }

    /**
     * @brief Checks if the buffer is active for its assigned token
     * @return True if the buffer will process when its token is processed
     */
    inline bool is_token_active() const override { return m_token_active; }

protected:
    /**
     * @brief Creates the default processor for this buffer type
     * @return Shared pointer to a ChannelProcessor
     *
     * Root buffers use a ChannelProcessor as their default processor,
     * which handles combining tributary buffers and node network output.
     */
    std::shared_ptr<BufferProcessor> create_default_processor() override;

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
     * @brief Flag indicating if this buffer is active for token processing
     */
    bool m_token_active;
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
 *
 * Token Compatibility:
 * - Primary Token: AUDIO_BACKEND (sample-rate, CPU, sequential processing)
 * - Compatible with other audio processing tokens through token compatibility rules
 * - Not compatible with GRAPHICS_BACKEND tokens due to different processing models
 */
class MAYAFLUX_API ChannelProcessor : public BufferProcessor {
public:
    /**
     * @brief Creates a new channel aggregation processor
     * @param root_buffer Shared pointer to the root buffer this processor will manage
     *
     * The processor maintains a raw pointer to its root buffer to avoid
     * circular references, as the root buffer already owns a shared_ptr
     * to this processor in the object composition hierarchy.
     */
    ChannelProcessor(const std::shared_ptr<Buffer>& root_buffer);

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
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when processor is attached to a buffer
     * @param buffer Buffer being attached to
     *
     * Validates that the buffer is a compatible RootAudioBuffer and ensures
     * token compatibility for proper processing pipeline integration.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Checks compatibility with a specific buffer type
     * @param buffer Buffer to check compatibility with
     * @return True if compatible (buffer is RootAudioBuffer), false otherwise
     */
    bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

private:
    /**
     * @brief Shared pointer to the root buffer this processor manages
     */
    std::shared_ptr<RootAudioBuffer> m_root_buffer;
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
 *
 * Token Compatibility:
 * - Primary Token: AUDIO_BACKEND (optimized for audio sample rate processing)
 * - Can adapt to GPU_PROCESS tokens for parallel limiting when beneficial
 * - Compatible with SEQUENTIAL and PARALLEL processing modes
 */
class MAYAFLUX_API FinalLimiterProcessor : public BufferProcessor {
public:
    /**
     * @brief Creates a new final limiter processor
     *
     * Initializes the processor with default AUDIO_BACKEND token for
     * standard audio processing compatibility.
     */
    FinalLimiterProcessor();

    /**
     * @brief Processes a buffer by enforcing boundary conditions
     * @param buffer Buffer to process
     *
     * This method applies a non-linear boundary enforcement algorithm to ensure
     * all values stay within the valid range (typically -1.0 to 1.0) before
     * being transmitted to hardware interfaces, while preserving the perceptual
     * characteristics of the original signal.
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when processor is attached to a buffer
     * @param buffer Buffer being attached to
     *
     * Validates that the buffer is an AudioBuffer-derived type and ensures
     * token compatibility for proper audio processing.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Checks compatibility with a specific buffer type
     * @param buffer Buffer to check compatibility with
     * @return True if compatible (buffer is AudioBuffer-derived), false otherwise
     */
    bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;
};

}
