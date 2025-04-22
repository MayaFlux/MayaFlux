#pragma once

#include "config.h"

namespace MayaFlux::Buffers {

class BufferProcessor;
class BufferProcessingChain;

/**
 * @class AudioBuffer
 * @brief Interface for sequential data storage and transformation
 *
 * AudioBuffer provides a common interface for all buffer types in the MayaFlux engine.
 * Buffers store sequential data samples and provide mechanisms for transforming
 * that data through attached processors. Unlike nodes which operate on individual values,
 * buffers process blocks of data, enabling efficient batch operations and transformations.
 *
 * Buffers can:
 * - Store and provide access to sequential numerical data
 * - Be transformed by one or more BufferProcessor objects
 * - Be arranged in processing networks via BufferProcessingChain
 * - Bridge between continuous (node) and discrete (buffer) computational domains
 *
 * The buffer system complements the node system by providing block-based processing
 * capabilities, which are more efficient for certain operations and essential for
 * interfacing with hardware and external systems that operate on data blocks.
 */
class AudioBuffer : public std::enable_shared_from_this<AudioBuffer> {
public:
    /**
     * @brief Virtual destructor for proper resource management
     */
    virtual ~AudioBuffer() = default;

    /**
     * @brief Initializes the buffer with specified channel and capacity
     * @param channel Channel identifier for this buffer
     * @param num_samples Buffer capacity in samples
     *
     * Sets up the buffer with the specified channel ID and allocates
     * memory for the specified number of samples.
     */
    virtual void setup(u_int32_t channel, u_int32_t num_samples) = 0;

    /**
     * @brief Adjusts the buffer's capacity
     * @param num_samples New buffer capacity
     *
     * Changes the buffer's capacity while preserving existing data
     * where possible. If the new size is smaller, data may be truncated.
     */
    virtual void resize(u_int32_t num_samples) = 0;

    /**
     * @brief Resets all data values in the buffer
     *
     * Initializes all sample values to zero without changing the buffer capacity.
     */
    virtual void clear() = 0;

    /**
     * @brief Gets the current capacity of the buffer
     * @return Buffer capacity in samples
     */
    virtual u_int32_t get_num_samples() const = 0;

    /**
     * @brief Gets mutable access to the buffer's underlying data
     * @return Reference to the vector containing sample data
     *
     * Provides direct access to the underlying data for reading
     * or modification. Use with caution as it bypasses any transformation chain.
     */
    virtual std::vector<double>& get_data() = 0;

    /**
     * @brief Gets read-only access to the buffer's underlying data
     * @return Const reference to the vector containing sample data
     */
    virtual const std::vector<double>& get_data() const = 0;

    /**
     * @brief Applies the default transformation to the buffer's data
     *
     * Executes the default processing algorithm on the buffer's data.
     * The specific transformation depends on the buffer type and its
     * configured default processor.
     */
    virtual void process_default() = 0;
    // virtual void process(const std::any& params) = 0;

    /**
     * @brief Gets the channel identifier for this buffer
     * @return Channel ID
     *
     * The channel ID typically corresponds to a logical data stream
     * in multi-channel systems.
     */
    virtual u_int32_t get_channel_id() const = 0;

    /**
     * @brief Sets the channel identifier for this buffer
     * @param id New channel ID
     */
    virtual void set_channel_id(u_int32_t id) = 0;

    /**
     * @brief Sets the capacity of the buffer
     * @param num_samples New buffer capacity
     *
     * Similar to resize(), but implementation may vary in derived classes.
     */
    virtual void set_num_samples(u_int32_t num_samples) = 0;

    /**
     * @brief Sets the default transformation processor for this buffer
     * @param processor Processor to use as default
     *
     * The default processor is used when process_default() is called.
     */
    virtual void set_default_processor(std::shared_ptr<BufferProcessor> processor) = 0;

    /**
     * @brief Gets the current default transformation processor
     * @return Shared pointer to the default processor
     */
    virtual std::shared_ptr<BufferProcessor> get_default_processor() const = 0;

    /**
     * @brief Gets the transformation chain attached to this buffer
     * @return Shared pointer to the buffer processing chain
     *
     * The processing chain contains multiple transformations that are
     * applied in sequence when the buffer is processed.
     */
    virtual std::shared_ptr<BufferProcessingChain> get_processing_chain() = 0;

    /**
     * @brief Sets the transformation chain for this buffer
     * @param chain New processing chain
     */
    virtual void set_processing_chain(std::shared_ptr<BufferProcessingChain> chain) = 0;

    /**
     * @brief Gets a reference to a specific value in the buffer
     * @param index Sample index
     * @return Reference to the value at the specified index
     *
     * Provides direct access to a specific value for reading or modification.
     * No bounds checking is performed, so the caller must ensure the index is valid.
     */
    virtual double& get_sample(u_int32_t index) = 0;
};

/**
 * @class StandardAudioBuffer
 * @brief Standard implementation of the AudioBuffer interface
 *
 * StandardAudioBuffer provides a concrete implementation of the AudioBuffer
 * interface with a vector-based storage for sequential numerical data.
 * It serves as the base class for more specialized buffer types and can be
 * used directly for general-purpose data storage and transformation.
 *
 * This class implements all the required methods from the AudioBuffer interface
 * and adds some additional functionality specific to standard buffers.
 */
class StandardAudioBuffer : public AudioBuffer {
public:
    /**
     * @brief Creates a new uninitialized buffer
     *
     * Initializes a buffer with no channel assignment and no allocated capacity.
     * The buffer must be set up with setup() before use.
     */
    StandardAudioBuffer();

    /**
     * @brief Creates a new buffer with specified channel and capacity
     * @param channel_id Channel identifier for this buffer
     * @param num_samples Buffer capacity in samples (default: 512)
     *
     * Initializes a buffer with the specified channel ID and allocates
     * memory for the specified number of samples.
     */
    StandardAudioBuffer(u_int32_t channel_id, u_int32_t num_samples = 512);

    /**
     * @brief Virtual destructor for proper resource management
     */
    ~StandardAudioBuffer() override = default;

    /**
     * @brief Initializes the buffer with specified channel and capacity
     * @param channel Channel identifier for this buffer
     * @param num_samples Buffer capacity in samples
     */
    void setup(u_int32_t channel, u_int32_t num_samples) override;

    /**
     * @brief Adjusts the buffer's capacity
     * @param num_samples New buffer capacity
     */
    void resize(u_int32_t num_samples) override;

    /**
     * @brief Resets all data values in the buffer
     */
    void clear() override;

    /**
     * @brief Gets the current capacity of the buffer
     * @return Buffer capacity in samples
     */
    u_int32_t get_num_samples() const override { return m_num_samples; }

    /**
     * @brief Gets the channel identifier for this buffer
     * @return Channel ID
     */
    u_int32_t get_channel_id() const override { return m_channel_id; }

    /**
     * @brief Sets the channel identifier for this buffer
     * @param id New channel ID
     */
    void set_channel_id(u_int32_t id) override { m_channel_id = id; }

    /**
     * @brief Sets the capacity of the buffer
     * @param num_samples New buffer capacity
     */
    void set_num_samples(u_int32_t num_samples) override;

    /**
     * @brief Gets mutable access to the buffer's underlying data
     * @return Reference to the vector containing sample data
     */
    std::vector<double>& get_data() override { return m_data; }

    /**
     * @brief Gets read-only access to the buffer's underlying data
     * @return Const reference to the vector containing sample data
     */
    const std::vector<double>& get_data() const override { return m_data; }

    /**
     * @brief Applies the default transformation to the buffer's data
     *
     * If no default processor is set, this may do nothing or use
     * a fallback algorithm depending on the implementation.
     */
    void process_default() override;
    // void process(const std::any& params) override;

    /**
     * @brief Sets the default transformation processor for this buffer
     * @param processor Processor to use as default
     */
    void set_default_processor(std::shared_ptr<BufferProcessor> processor) override;

    /**
     * @brief Gets the current default transformation processor
     * @return Shared pointer to the default processor
     */
    std::shared_ptr<BufferProcessor> get_default_processor() const override { return m_default_processor; }

    /**
     * @brief Gets the transformation chain attached to this buffer
     * @return Shared pointer to the buffer processing chain
     */
    std::shared_ptr<BufferProcessingChain> get_processing_chain() override { return m_processing_chain.lock(); }

    /**
     * @brief Sets the transformation chain for this buffer
     * @param chain New processing chain
     */
    void set_processing_chain(std::shared_ptr<BufferProcessingChain> chain) override;

    /**
     * @brief Gets a reference to a specific value in the buffer
     * @param index Sample index
     * @return Reference to the value at the specified index
     */
    inline virtual double& get_sample(u_int32_t index) override { return get_data()[index]; }

protected:
    /**
     * @brief Channel identifier for this buffer
     *
     * Typically corresponds to a logical data stream in multi-channel systems.
     */
    u_int32_t m_channel_id;

    /**
     * @brief Capacity of the buffer in samples
     */
    u_int32_t m_num_samples;

    /**
     * @brief Vector storing the actual numerical data
     */
    std::vector<double> m_data;

    /**
     * @brief Sample rate for this buffer (default: 48000 Hz)
     */
    u_int32_t m_sample_rate = 48000;

    /**
     * @brief Default transformation processor for this buffer
     *
     * Used when process_default() is called.
     */
    std::shared_ptr<BufferProcessor> m_default_processor;

    /**
     * @brief Weak reference to the transformation chain
     *
     * Using a weak_ptr prevents circular references between
     * the buffer and its processing chain.
     */
    std::weak_ptr<BufferProcessingChain> m_processing_chain;

    /**
     * @brief Creates a default transformation processor for this buffer type
     * @return Shared pointer to the created processor, or nullptr if none
     *
     * This method is called when a default processor is needed but none
     * has been explicitly set. The base implementation returns nullptr,
     * but derived classes can override this to provide type-specific
     * default processors.
     */
    virtual std::shared_ptr<BufferProcessor> create_default_processor() { return nullptr; }
};
}
