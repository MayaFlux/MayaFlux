#pragma once

#include "Buffer.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Buffers {

/**
 * @struct BufferRoutingState
 * @brief Represents the state of routing transitions for a buffer
 *
 * Tracks fade-in and fade-out state when routing a buffer from one channel to another.
 * Unlike nodes, buffers only support 1-to-1 routing due to their single channel_id architecture.
 */
struct BufferRoutingState {
    double from_amount { 1.0 };
    double to_amount { 0.0 };
    uint32_t cycles_elapsed {};
    uint32_t from_channel {};
    uint32_t to_channel {};
    uint32_t fade_cycles {};

    enum Phase : uint8_t {
        NONE = 0x00,
        ACTIVE = 0x01,
        COMPLETED = 0x02
    } phase { Phase::NONE };
};

/**
 * @class AudioBuffer
 * @brief Concrete audio implementation of the Buffer interface for double-precision audio data
 *
 * AudioBuffer provides the primary concrete implementation for audio data storage and processing
 * in the MayaFlux engine. It specializes the generic Buffer interface for audio-specific operations,
 * storing sequential audio samples as double-precision floating-point values and providing
 * optimized processing capabilities for audio backends.
 *
 * This implementation serves as the foundation for audio processing in MayaFlux and is the
 * preferred buffer type for audio backends due to its optimized memory layout and processing
 * characteristics. Other specialized audio buffer types can inherit from AudioBuffer to extend
 * its functionality while maintaining compatibility with the audio processing pipeline.
 *
 * AudioBuffer capabilities:
 * - Store and manage sequential double-precision audio samples
 * - Support multi-channel audio through channel identification
 * - Provide efficient block-based audio processing via BufferProcessor objects
 * - Integrate with BufferProcessingChain for complex audio transformation pipelines
 * - Bridge between continuous audio streams (nodes) and discrete audio blocks (buffers)
 * - Support dynamic buffer lifecycle management for streaming audio applications
 *
 * The AudioBuffer complements the node system by providing block-based audio processing
 * capabilities essential for real-time audio processing, hardware interfacing, and
 * computationally intensive audio transformations that benefit from batch operations.
 */
class MAYAFLUX_API AudioBuffer : public Buffer {
public:
    /**
     * @brief Creates a new uninitialized audio buffer
     *
     * Initializes an audio buffer with no channel assignment and no allocated capacity.
     * The buffer must be configured with setup() before use. This constructor is useful
     * when buffer parameters will be determined at runtime.
     */
    AudioBuffer();

    /**
     * @brief Creates a new audio buffer with specified channel and capacity
     * @param channel_id Audio channel identifier for this buffer
     * @param num_samples Buffer capacity in audio samples (default: 512)
     *
     * Initializes an audio buffer with the specified channel ID and allocates
     * memory for the specified number of double-precision audio samples.
     * The default capacity of 512 samples is optimized for typical audio
     * processing block sizes.
     */
    AudioBuffer(uint32_t channel_id, uint32_t num_samples = 512);

    /**
     * @brief Virtual destructor for proper resource management
     *
     * Ensures proper cleanup of audio buffer resources and any attached
     * audio processors when the buffer is destroyed.
     */
    ~AudioBuffer() override = default;

    /**
     * @brief Initializes the audio buffer with specified channel and capacity
     * @param channel Audio channel identifier for this buffer
     * @param num_samples Buffer capacity in audio samples
     *
     * Configures the audio buffer with the specified channel ID and allocates
     * memory for the specified number of audio samples. This method should be
     * called on uninitialized buffers before use.
     */
    virtual void setup(uint32_t channel, uint32_t num_samples);

    /**
     * @brief Sets up audio processors for the specified processing token
     * @param token Processing token indicating the domain
     *
     * This is useful to avoid calling shared_from_this() in constructors of
     * derived classes. Derived audio buffer types can override this method
     * to attach audio-specific processors based on the processing token.
     */
    virtual void setup_processors(ProcessingToken token) { }

    /**
     * @brief Adjusts the audio buffer's sample capacity
     * @param num_samples New buffer capacity in audio samples
     *
     * Changes the buffer's capacity while preserving existing audio data
     * where possible. If the new size is smaller, audio data may be truncated.
     * This operation maintains audio continuity when possible.
     */
    virtual void resize(uint32_t num_samples);

    /**
     * @brief Resets all audio samples in the buffer to silence
     *
     * Initializes all audio sample values to zero (silence) without changing
     * the buffer capacity. This is the audio-specific implementation of the
     * Buffer interface's clear() method.
     */
    void clear() override;

    /**
     * @brief Gets the current capacity of the audio buffer
     * @return Buffer capacity in audio samples
     *
     * Returns the number of double-precision audio samples that can be
     * stored in this buffer.
     */
    inline virtual uint32_t get_num_samples() const { return m_num_samples; }

    /**
     * @brief Gets mutable access to the buffer's underlying audio data
     * @return Reference to the vector containing audio sample data
     *
     * Provides direct access to the underlying double-precision audio data
     * for reading or modification. Use with caution as it bypasses any
     * audio transformation chain that may be configured.
     */
    inline virtual std::vector<double>& get_data() { return m_data; }

    /**
     * @brief Gets read-only access to the buffer's underlying audio data
     * @return Const reference to the vector containing audio sample data
     *
     * Provides read-only access to the underlying double-precision audio
     * samples for analysis or copying without modification risk.
     */
    inline virtual const std::vector<double>& get_data() const { return m_data; }

    /**
     * @brief Applies the default audio transformation to the buffer's data
     *
     * Executes the default audio processing algorithm on the buffer's sample data.
     * The specific transformation depends on the configured default audio processor,
     * which may perform operations like normalization, filtering, or effects processing.
     */
    void process_default() override;

    /**
     * @brief Gets the audio channel identifier for this buffer
     * @return Audio channel ID
     *
     * Returns the audio channel identifier, which typically corresponds to a
     * logical audio stream in multi-channel audio systems (e.g., left/right
     * for stereo, or numbered channels in surround sound configurations).
     */
    virtual uint32_t get_channel_id() const { return m_channel_id; }

    /**
     * @brief Sets the audio channel identifier for this buffer
     * @param id New audio channel ID
     *
     * Updates the channel identifier for this audio buffer, allowing it to
     * be reassigned to different audio channels in multi-channel configurations.
     */
    inline void set_channel_id(uint32_t id) { m_channel_id = id; }

    /**
     * @brief Sets the capacity of the audio buffer
     * @param num_samples New buffer capacity in audio samples
     *
     * Updates the buffer's sample capacity. Similar to resize(), but the
     * implementation may vary in derived audio buffer classes to provide
     * specialized behavior for different audio buffer types.
     */
    virtual void set_num_samples(uint32_t num_samples);

    /**
     * @brief Sets the default audio transformation processor for this buffer
     * @param processor Audio processor to use as default transformation
     *
     * Configures the default audio processor that will be used when
     * process_default() is called. The processor should be compatible
     * with double-precision audio data processing.
     */
    void set_default_processor(const std::shared_ptr<BufferProcessor>& processor) override;

    /**
     * @brief Gets the current default audio transformation processor
     * @return Shared pointer to the default audio processor
     *
     * Returns the audio processor that will be used for default transformations
     * on this buffer's audio data, or nullptr if no default processor is set.
     */
    inline std::shared_ptr<BufferProcessor> get_default_processor() const override { return m_default_processor; }

    /**
     * @brief Gets the audio transformation chain attached to this buffer
     * @return Shared pointer to the audio buffer processing chain
     *
     * Returns the processing chain that contains multiple audio transformations
     * applied in sequence when the buffer is processed. This enables complex
     * audio processing pipelines for effects, filtering, and analysis.
     */
    inline std::shared_ptr<BufferProcessingChain> get_processing_chain() override { return m_processing_chain; }

    /**
     * @brief Sets the audio transformation chain for this buffer
     * @param chain New audio processing chain for sequential transformations
     * @param force If true, replaces the existing chain even if one is already set
     *
     * Replaces the current audio processing chain with the provided one.
     * The chain should contain processors compatible with double-precision
     * audio data.
     */
    void set_processing_chain(const std::shared_ptr<BufferProcessingChain>& chain, bool force = false) override;

    /**
     * @brief Gets a reference to a specific audio sample in the buffer
     * @param index Audio sample index
     * @return Reference to the audio sample value at the specified index
     *
     * Provides direct access to a specific audio sample for reading or modification.
     * No bounds checking is performed, so the caller must ensure the index is valid
     * within the buffer's capacity.
     */
    inline virtual double& get_sample(uint32_t index) { return get_data()[index]; }

    /**
     * @brief Checks if the audio buffer has data for the current processing cycle
     * @return True if the buffer has audio data for processing, false otherwise
     *
     * This method is particularly relevant when using SignalSourceContainers or
     * streaming audio systems. Standard audio buffers typically return true unless
     * explicitly marked otherwise, but derived classes may implement different
     * behavior based on streaming state or data availability.
     */
    inline bool has_data_for_cycle() const override { return m_has_data; }

    /**
     * @brief Checks if the buffer should be removed from the processing chain
     * This is relevant when using SignalSourceContainers. Standard audio buffers will
     * always return false unless specified otherwise by their derived class implementations.
     * @return True if the buffer should be removed, false otherwise
     *
     * This method enables dynamic audio buffer lifecycle management. Standard audio
     * buffers typically return false unless explicitly marked for removal, but
     * derived classes may implement automatic removal based on streaming end
     * conditions or resource management policies.
     */
    inline bool needs_removal() const override { return m_should_remove; }

    /**
     * @brief Marks the audio buffer for processing in the current cycle
     * @param has_data True if the buffer has audio data to process, false otherwise
     *
     * This method allows external audio systems to control whether the buffer should
     * be considered for processing in the current audio cycle. Standard audio buffers
     * are typically always marked as having data unless explicitly disabled.
     */
    inline void mark_for_processing(bool has_data) override { m_has_data = has_data; }

    /**
     * @brief Marks the audio buffer for removal from processing chains
     *
     * Sets the buffer's removal flag, indicating it should be removed from
     * any audio processing chains or management systems. Standard audio buffers
     * rarely need removal unless explicitly requested by the application or
     * when audio streams end.
     */
    inline void mark_for_removal() override { m_should_remove = true; }

    /**
     * @brief Controls whether the audio buffer should use default processing
     * @param should_process True if default audio processing should be applied, false otherwise
     *
     * This method allows fine-grained control over when the buffer's default
     * audio processor is applied. Standard audio buffers typically always use
     * default processing unless specific audio processing requirements dictate
     * otherwise.
     */
    inline void enforce_default_processing(bool should_process) override { m_process_default = should_process; }

    /**
     * @brief Checks if the audio buffer should undergo default processing
     * @return True if default audio processing should be applied, false otherwise
     *
     * Determines whether the buffer's default audio processor should be executed
     * during the current processing cycle. Standard audio buffers typically always
     * need default processing unless specifically configured otherwise for
     * specialized audio processing scenarios.
     */
    inline bool needs_default_processing() override { return m_process_default; }

    /**
     * @brief Attempts to acquire processing rights for the buffer
     * @return True if processing rights were successfully acquired, false otherwise
     *
     * This method is used to control access to the buffer's data during processing.
     * It allows the buffer to manage concurrent access and ensure that only one
     * processing operation occurs at a time. The specific implementation may vary
     * based on the buffer type and its processing backend.
     */
    inline bool try_acquire_processing() override
    {
        bool expected = false;
        return m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed);
    }

    /**
     * @brief Releases processing rights for the buffer
     *
     * This method is called to release the processing rights acquired by
     * try_acquire_processing(). It allows other processing operations to
     * access the buffer's data once the current operation is complete.
     * The specific implementation may vary based on the buffer type and
     * its processing backend.
     */
    inline void release_processing() override
    {
        m_is_processing.store(false, std::memory_order_release);
    }

    /**
     * @brief Checks if the buffer is currently being processed
     * @return True if the buffer is in a processing state, false otherwise
     *
     * This method indicates whether the buffer is currently undergoing
     * a processing operation. It is used to manage concurrent access and
     * ensure that processing operations do not interfere with each other.
     * The specific implementation may vary based on the buffer type and
     * its processing backend.
     */
    inline bool is_processing() const override
    {
        return m_is_processing.load(std::memory_order_acquire);
    }

    /**
     * @brief Creates a clone of this audio buffer for a specific channel
     * @param channel Channel identifier for the cloned buffer
     * @return Shared pointer to the cloned audio buffer
     *
     * This method creates a new instance of the audio buffer with the same
     * data and properties, but assigned to a different audio channel. The
     * cloned buffer can be used independently in audio processing chains.
     *
     * NOTE: The moment of cloning is the divergence point between the original
     * and the cloned. While they both will follow the same processing chain or have the same
     * default procesor, any changes made to one buffer after cloning will not affect the other.
     */
    virtual std::shared_ptr<AudioBuffer> clone_to(uint32_t channel);

    std::shared_ptr<Buffer> clone_to(uint8_t dest_desc) override;

    /**
     * @brief Reads audio data into the buffer from the audio backend
     * @param buffer Shared pointer to the AudioBuffer to read data into
     * @param force Whether to force reading even if in processing state
     * @return True if data was successfully read, false otherwise
     *
     * This method attempts to read audio data from the audio backend into the
     * provided AudioBuffer. If force is true, it will attempt to read even if
     * the current buffer or the copy buffer are in processing state
     */
    virtual bool read_once(const std::shared_ptr<AudioBuffer>& buffer, bool force = false);

    /**
     * @brief Marks the buffer as internal-only, preventing root aggregation
     * @param internal True to mark as internal-only, false to allow root aggregation
     *
     * Internal-only buffers are excluded from root-level aggregation and
     * processing. This is typically used for buffers that are processed
     * entirely within a specific backend or domain (e.g., GPU-only buffers).
     */
    void force_internal_usage(bool internal) override { m_internal_usage = internal; }

    /**
     * @brief Checks if the buffer is marked as internal-only
     * @return True if the buffer is internal-only, false otherwise
     *
     * Indicates whether the buffer is excluded from root-level aggregation
     * and processing. Internal-only buffers are typically processed entirely
     * within a specific backend or domain.
     */
    bool is_internal_only() const override { return m_internal_usage; }

    /**
     * @brief Retrieves the current routing state of the buffer
     * @return Reference to the current BufferRoutingState structure
     */
    [[nodiscard]] const BufferRoutingState& get_routing_state() const { return m_routing_state; }

    /**
     * @brief Retrieves the current routing state of the buffer (non-const)
     * @return Reference to the current BufferRoutingState structure
     */
    BufferRoutingState& get_routing_state() { return m_routing_state; }

    /**
     * @brief Checks if the buffer is currently in a routing transition phase
     * @return true if the buffer is in an active or completed routing phase
     */
    [[nodiscard]] bool needs_routing() const
    {
        return m_routing_state.phase & (BufferRoutingState::ACTIVE | BufferRoutingState::COMPLETED);
    }

protected:
    /**
     * @brief Audio channel identifier for this buffer
     *
     * Identifies which audio channel this buffer represents in multi-channel
     * audio systems. Typically corresponds to logical audio channels like
     * left/right for stereo or numbered channels in surround configurations.
     */
    uint32_t m_channel_id;

    /**
     * @brief Capacity of the buffer in audio samples
     *
     * Stores the maximum number of double-precision audio samples this
     * buffer can contain. This determines the buffer's memory allocation
     * and processing block size.
     */
    uint32_t m_num_samples;

    /**
     * @brief Vector storing the actual double-precision audio sample data
     *
     * Contains the raw audio sample data as double-precision floating-point
     * values. This provides high precision for audio processing while
     * maintaining compatibility with most audio processing algorithms.
     */
    std::vector<double> m_data;

    /**
     * @brief Default audio transformation processor for this buffer
     *
     * Stores the audio processor that will be used when process_default()
     * is called. This enables automatic audio processing without explicit
     * processor management.
     */
    std::shared_ptr<BufferProcessor> m_default_processor;

    /**
     * @brief Audio transformation processing chain for this buffer
     *
     * Contains a sequence of audio processors that will be applied in order
     */
    std::shared_ptr<BufferProcessingChain> m_processing_chain;

    /**
     * @brief Creates a default audio transformation processor for this buffer type
     * @return Shared pointer to the created audio processor, or nullptr if none
     *
     * This method is called when a default audio processor is needed but none
     * has been explicitly set. The base AudioBuffer implementation returns nullptr,
     * but derived audio buffer classes can override this to provide type-specific
     * default audio processors.
     */
    virtual std::shared_ptr<BufferProcessor> create_default_processor() { return nullptr; }

    /**
     * @brief Whether the audio buffer has data to process this cycle
     *
     * Tracks whether this audio buffer contains valid audio data for the
     * current processing cycle. Relevant for streaming audio systems and
     * dynamic buffer management.
     */
    bool m_has_data;

    /**
     * @brief Whether the audio buffer should be removed from processing chains
     *
     * Indicates whether this audio buffer should be removed from any
     * processing chains or management systems. Used for dynamic audio
     * buffer lifecycle management.
     */
    bool m_should_remove;

    /**
     * @brief Whether the audio buffer should be processed using its default processor
     *
     * Controls whether the buffer's default audio processor should be
     * applied during processing cycles. Allows for selective audio
     * processing based on current requirements.
     */
    bool m_process_default;

    /**
     * @brief Internal state tracking for routing transitions
     */
    BufferRoutingState m_routing_state;

private:
    std::atomic<bool> m_is_processing;

    bool m_internal_usage {};
};

}
