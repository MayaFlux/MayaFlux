#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;
class Buffer;
class VKBuffer;
class RootAudioBuffer;
class RootGraphicsBuffer;
class BufferProcessingChain;
class TokenUnitManager;

/**
 * @class BufferAccessControl
 * @brief Token-aware buffer and unit access patterns
 *
 * Manages all operations related to accessing and querying buffer data, units,
 * and processing chains. Works directly with a TokenUnitManager that it holds
 * as a member, providing a cohesive interface for buffer access operations.
 *
 * Design Principles:
 * - Member variable: Holds reference to TokenUnitManager
 * - Token-aware: Works with any token's audio or graphics units
 * - Domain-agnostic: Doesn't distinguish between audio/graphics at logic level
 * - Single responsibility: Only handles access/query/resize, not processing
 *
 * This class is a facade over the unit manager's storage, providing convenient
 * methods for the common buffer access patterns BufferManager needs.
 */
class MAYAFLUX_API BufferAccessControl {
public:
    /**
     * @brief Creates a new access control handler
     * @param unit_manager Reference to the TokenUnitManager
     */
    explicit BufferAccessControl(TokenUnitManager& unit_manager);

    ~BufferAccessControl() = default;

    // =========================================================================
    // Audio Buffer Data Access
    // =========================================================================

    /**
     * @brief Gets mutable reference to audio buffer data for a channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the audio buffer's data vector
     */
    std::vector<double>& get_audio_buffer_data(ProcessingToken token, uint32_t channel);

    /**
     * @brief Gets const reference to audio buffer data for a channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Const reference to the audio buffer's data vector
     */
    [[nodiscard]] const std::vector<double>& get_audio_buffer_data(ProcessingToken token, uint32_t channel) const;

    // =========================================================================
    // Audio Channel and Sizing Operations
    // =========================================================================

    /**
     * @brief Gets the number of channels for an audio token
     * @param token Processing domain
     * @return Number of channels, or 0 if token not found
     */
    [[nodiscard]] uint32_t get_num_audio_out_channels(ProcessingToken token) const;

    /**
     * @brief Gets the buffer size for an audio token
     * @param token Processing domain
     * @return Buffer size in samples, or 512 if token not found
     */
    [[nodiscard]] uint32_t get_audio_buffer_size(ProcessingToken token) const;

    /**
     * @brief Resizes all audio buffers for a token to the specified size
     * @param token Processing domain
     * @param buffer_size New buffer size in samples
     */
    void resize_audio_buffers(ProcessingToken token, uint32_t buffer_size);

    /**
     * @brief Ensures minimum number of audio channels exist for a token
     * @param token Processing domain
     * @param channel_count Minimum number of channels to ensure
     */
    void ensure_audio_channels(ProcessingToken token, uint32_t channel_count);

    // =========================================================================
    // Root Buffer Access (Audio)
    // =========================================================================

    /**
     * @brief Gets the root audio buffer for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the root audio buffer
     */
    std::shared_ptr<RootAudioBuffer> get_root_audio_buffer(ProcessingToken token, uint32_t channel);

    /**
     * @brief Gets the root audio buffer for a specific token and channel (const)
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the root audio buffer
     */
    [[nodiscard]] std::shared_ptr<const RootAudioBuffer> get_root_audio_buffer(ProcessingToken token, uint32_t channel) const;

    // =========================================================================
    // Root Buffer Access (Graphics)
    // =========================================================================

    /**
     * @brief Gets the root graphics buffer for a specific token
     * @param token Processing domain
     * @return Shared pointer to the root graphics buffer
     */
    std::shared_ptr<RootGraphicsBuffer> get_root_graphics_buffer(ProcessingToken token);

    /**
     * @brief Gets the root graphics buffer for a specific token (const)
     * @param token Processing domain
     * @return Shared pointer to the root graphics buffer
     */
    [[nodiscard]] std::shared_ptr<const RootGraphicsBuffer> get_root_graphics_buffer(ProcessingToken token) const;

    // =========================================================================
    // Audio Buffer Hierarchy Management
    // =========================================================================

    /**
     * @brief Adds a buffer to a token (dispatches based on token type)
     * @param buffer Buffer to add (AudioBuffer or VKBuffer)
     * @param token Processing domain
     * @param channel Channel index (used only for audio tokens)
     */
    void add_buffer(
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken token,
        uint32_t channel = 0);

    /**
     * @brief Adds an audio buffer to a token and channel
     * @param buffer Audio buffer to add
     * @param token Processing domain
     * @param channel Channel index
     */
    void add_audio_buffer(
        const std::shared_ptr<AudioBuffer>& buffer,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Removes a buffer from a token (dispatches based on token type)
     * @param buffer Buffer to remove
     * @param token Processing domain
     * @param channel Channel index (used only for audio tokens)
     */
    void remove_buffer(
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken token,
        uint32_t channel = 0);

    /**
     * @brief Removes an audio buffer from a token and channel
     * @param buffer Audio buffer to remove
     * @param token Processing domain
     * @param channel Channel index
     */
    void remove_audio_buffer(
        const std::shared_ptr<AudioBuffer>& buffer,
        ProcessingToken token,
        uint32_t channel);

    /**
     * @brief Gets all audio buffers for a token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Const reference to vector of audio buffers
     */
    [[nodiscard]] const std::vector<std::shared_ptr<AudioBuffer>>& get_audio_buffers(
        ProcessingToken token,
        uint32_t channel) const;

    // =========================================================================
    // Graphics Buffer Hierarchy Management
    // =========================================================================

    /**
     * @brief Adds a graphics buffer to a token
     * @param buffer Graphics buffer to add (must be VKBuffer-derived)
     * @param token Processing domain
     */
    void add_graphics_buffer(const std::shared_ptr<Buffer>& buffer, ProcessingToken token);

    /**
     * @brief Removes a graphics buffer from a token
     * @param buffer Graphics buffer to remove
     * @param token Processing domain
     */
    void remove_graphics_buffer(const std::shared_ptr<Buffer>& buffer, ProcessingToken token);

    /**
     * @brief Gets all graphics buffers for a token
     * @param token Processing domain
     * @return Const reference to vector of graphics buffers
     */
    [[nodiscard]] const std::vector<std::shared_ptr<VKBuffer>>& get_graphics_buffers(ProcessingToken token) const;

    /**
     * @brief Gets graphics buffers filtered by usage type
     * @param usage VKBuffer::Usage type to filter by
     * @param token Processing domain
     * @return Vector of buffers matching the specified usage
     */
    [[nodiscard]] std::vector<std::shared_ptr<VKBuffer>> get_graphics_buffers_by_usage(
        VKBuffer::Usage usage,
        ProcessingToken token) const;

    // =========================================================================
    // Processing Chain Access
    // =========================================================================

    /**
     * @brief Gets the processing chain for an audio token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Shared pointer to the processing chain
     */
    std::shared_ptr<BufferProcessingChain> get_audio_processing_chain(ProcessingToken token, uint32_t channel);

    /**
     * @brief Gets the processing chain for a graphics token
     * @param token Processing domain
     * @return Shared pointer to the processing chain
     */
    std::shared_ptr<BufferProcessingChain> get_graphics_processing_chain(ProcessingToken token);

    /**
     * @brief Initializes the buffer service for Vulkan operations
     */
    void initialize_buffer_service();

private:
    /// Reference to the token/unit manager for storage operations
    TokenUnitManager& m_unit_manager;

    /// Vulkan buffer processing context
    Registry::Service::BufferService* m_buffer_service = nullptr;
};

} // namespace MayaFlux::Buffers
