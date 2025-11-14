#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;
class TokenUnitManager;
class BufferAccessControl;

/**
 * @class BufferSupplyMixing
 * @brief External buffer supply, mixing, and interleaved data I/O
 *
 * Manages operations for supplying external buffers to channels, mixing buffers,
 * and converting between interleaved and channel-separated formats. This enables
 * advanced routing patterns where the same buffer feeds multiple channels,
 * or data is imported/exported in different formats.
 *
 * Design Principles:
 * - Token-aware: Routes operations to appropriate tokens
 * - Format-agnostic: Handles both channel-separated and interleaved data
 * - Mixing coordination: Uses MixProcessor for proper signal combining
 * - Single responsibility: Only handles supply/mixing/format operations
 *
 * This class encapsulates all buffer supply and data I/O logic.
 */
class MAYAFLUX_API BufferSupplyMixing {
public:
    /**
     * @brief Creates a new supply/mixing control handler
     * @param unit_manager Reference to the TokenUnitManager
     * @param access_control Reference to the BufferAccessControl
     */
    BufferSupplyMixing(TokenUnitManager& unit_manager, BufferAccessControl& access_control);

    ~BufferSupplyMixing() = default;

    // =========================================================================
    // Buffer Supply and Mixing
    // =========================================================================

    /**
     * @brief Supplies an external audio buffer to a specific token and channel
     * @param buffer Buffer to supply
     * @param token Processing domain
     * @param channel Channel index
     * @param mix Mix level (default: 1.0)
     * @return True if the buffer was successfully supplied, false otherwise
     *
     * The buffer data is added, mixed, and normalized at the end of the processing
     * chain of the channel's root buffer, but before final processing. This is
     * useful when one AudioBuffer needs to be supplied to multiple channels.
     */
    bool supply_audio_buffer_to(
        const std::shared_ptr<AudioBuffer>& buffer,
        ProcessingToken token,
        uint32_t channel,
        double mix = 1.0);

    /**
     * @brief Removes a previously supplied buffer from a token and channel
     * @param buffer Buffer to remove
     * @param token Processing domain
     * @param channel Channel index
     * @return True if the buffer was successfully removed, false otherwise
     *
     * Cleans up the mixing relationship between the supplied buffer and the target channel.
     */
    bool remove_supplied_audio_buffer(
        const std::shared_ptr<AudioBuffer>& buffer,
        ProcessingToken token,
        uint32_t channel);

    // =========================================================================
    // Interleaved Data I/O
    // =========================================================================

    /**
     * @brief Fills audio token channels from interleaved source data
     * @param interleaved_data Source interleaved data buffer
     * @param num_frames Number of frames to process
     * @param token Processing domain
     * @param num_channels Number of channels in the interleaved data
     *
     * Takes interleaved data (like typical audio file format or hardware I/O)
     * and distributes it to the token's channels.
     */
    void fill_audio_from_interleaved(
        const double* interleaved_data,
        uint32_t num_frames,
        ProcessingToken token,
        uint32_t num_channels);

    /**
     * @brief Fills interleaved buffer from audio token channels
     * @param interleaved_data Target interleaved data buffer
     * @param num_frames Number of frames to process
     * @param token Processing domain
     * @param num_channels Number of channels to interleave
     *
     * Takes channel-separated data from the token and interleaves it into
     * a single buffer (like typical audio file format or hardware I/O).
     */
    void fill_audio_interleaved(
        double* interleaved_data,
        uint32_t num_frames,
        ProcessingToken token,
        uint32_t num_channels) const;

    // =========================================================================
    // Buffer Cloning
    // =========================================================================

    /**
     * @brief Clones an audio buffer for each channel in the specified list
     * @param buffer Buffer to clone
     * @param channels Vector of channel indices to clone for
     * @param token Processing domain
     *
     * Creates a new buffer for each specified channel, copying the structure
     * but maintaining independent data. Useful for multi-channel processing
     * where each channel needs its own processing chain.
     */
    std::vector<std::shared_ptr<AudioBuffer>> clone_audio_buffer_for_channels(
        const std::shared_ptr<AudioBuffer>& buffer,
        const std::vector<uint32_t>& channels,
        ProcessingToken token);

private:
    /// Reference to the token/unit manager
    TokenUnitManager& m_unit_manager;

    /// Reference to the buffer access control
    BufferAccessControl& m_access_control;
};

} // namespace MayaFlux::Buffers
