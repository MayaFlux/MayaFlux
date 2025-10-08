#pragma once
#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class InputAudioBuffer
 * @brief Specialized buffer for audio input with listener dispatch
 *
 * Simple, focused input buffer that:
 * - Receives input data from audio interface
 * - Dispatches data to registered listener buffers
 * - Maintains clean, stable operation without complex features
 */
class InputAudioBuffer : public AudioBuffer {
public:
    /**
     * @brief Constructor - only BufferManager can create
     * @param channel_id Channel identifier
     * @param num_samples Buffer size
     */
    InputAudioBuffer(uint32_t channel_id, uint32_t num_samples);

    /**
     * @brief Deleted copy constructor - input buffers are unique resources
     */
    InputAudioBuffer(const InputAudioBuffer&) = delete;
    InputAudioBuffer& operator=(const InputAudioBuffer&) = delete;

    /**
     * @brief Move constructor for resource management
     */
    InputAudioBuffer(InputAudioBuffer&&) = delete;
    InputAudioBuffer& operator=(InputAudioBuffer&&) = delete;

    ~InputAudioBuffer() = default;

    /**
     * @brief Writes buffer data to a specific listener buffer
     * @param buffer Target buffer to write to
     */
    void write_to(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Registers a buffer as a listener of this input
     * @param buffer Buffer to receive input data
     */
    void register_listener(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Unregisters a listener buffer
     * @param buffer Buffer to unregister
     */
    void unregister_listener(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Gets all registered listeners
     * @return Vector of listener buffers
     */
    const std::vector<std::shared_ptr<AudioBuffer>>& get_listeners() const;

    /**
     * @brief Clears all registered listeners
     */
    void clear_listeners();
};

/**
 * @class InputAccessProcessor
 * @brief Simple processor for dispatching input data to listeners
 *
 * Handles the distribution of input data to registered listener buffers.
 * No complex features - just clean, stable dispatch.
 */
class InputAccessProcessor : public BufferProcessor {
public:
    /**
     * @brief Main processing function - dispatches data to listeners
     * @param buffer Input buffer to process
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when processor is attached to a buffer
     * @param buffer Buffer being attached to
     */
    void on_attach(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Checks compatibility with buffer type
     * @param buffer Buffer to check
     * @return True if compatible with InputAudioBuffer
     */
    bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override;

    /**
     * @brief Adds a listener buffer
     * @param buffer Listener buffer
     */
    void add_listener(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Removes a listener buffer
     * @param buffer Listener buffer to remove
     */
    void remove_listener(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Gets the number of registered listeners
     * @return Number of listeners
     */
    size_t get_listener_count() const { return m_listeners.size(); }

    inline void clear_listeners() { m_listeners.clear(); }

private:
    std::vector<std::shared_ptr<AudioBuffer>> m_listeners;
};
}
