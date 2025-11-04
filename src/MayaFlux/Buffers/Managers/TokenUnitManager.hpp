#pragma once

#include "MayaFlux/Buffers/Root/RootAudioBuffer.hpp"
#include "MayaFlux/Buffers/Root/RootGraphicsBuffer.hpp"

namespace MayaFlux::Buffers {

using RootAudioProcessingFunction = std::function<void(std::vector<std::shared_ptr<RootAudioBuffer>>&, uint32_t)>;
using RootGraphicsProcessingFunction = std::function<void(std::shared_ptr<RootGraphicsBuffer>&, uint32_t)>;

/**
 * @brief Represents a root audio unit containing buffers and processing chains for multiple channels
 */
struct RootAudioUnit {
    std::vector<std::shared_ptr<RootAudioBuffer>> root_buffers;
    std::vector<std::shared_ptr<BufferProcessingChain>> processing_chains;
    RootAudioProcessingFunction custom_processor;
    uint32_t channel_count = 0;
    uint32_t buffer_size = 512;

    [[nodiscard]] std::shared_ptr<RootAudioBuffer> get_buffer(uint32_t channel) const
    {
        return root_buffers[channel];
    }

    [[nodiscard]] std::shared_ptr<BufferProcessingChain> get_chain(uint32_t channel) const
    {
        return processing_chains[channel];
    }

    void resize_channels(uint32_t new_count, uint32_t new_buffer_size, ProcessingToken token);

    void resize_buffers(uint32_t new_buffer_size);
};

/**
 * @brief Represents a root graphics unit containing a buffer and processing chain
 */
struct RootGraphicsUnit {
    std::shared_ptr<RootGraphicsBuffer> root_buffer;
    std::shared_ptr<BufferProcessingChain> processing_chain;
    uint32_t frame_count = 0; // For tracking processed frames
    RootGraphicsProcessingFunction custom_processor;

    RootGraphicsUnit();

    void initialize(ProcessingToken token);

    [[nodiscard]] std::shared_ptr<RootGraphicsBuffer> get_buffer() const { return root_buffer; }

    [[nodiscard]] std::shared_ptr<BufferProcessingChain> get_chain() const { return processing_chain; }
};

/**
 * @class TokenUnitManager
 * @brief Token-scoped unit storage and lifecycle management
 *
 * Manages the core data structures for both audio and graphics units, providing
 * thread-safe, token-aware access patterns. This class is the single source of truth
 * for unit lifecycle and storage.
 *
 * Design Principles:
 * - Token-generic: Doesn't distinguish between audio/graphics at the data level
 * - Thread-safe: All modifications guarded by mutex
 * - Lazy creation: Units created on-demand via get_or_create patterns
 * - Immutable queries: Const queries never create units
 */
class MAYAFLUX_API TokenUnitManager {
public:
    /**
     * @brief Creates a new unit manager with initial audio unit configuration
     * @param default_token Primary processing token (typically AUDIO_BACKEND)
     * @param default_out_channels Output channels for default token
     * @param default_buffer_size Buffer size for default token
     */
    TokenUnitManager(
        ProcessingToken default_token,
        uint32_t default_out_channels,
        uint32_t default_buffer_size);

    TokenUnitManager(
        ProcessingToken default_audio_token = ProcessingToken::AUDIO_BACKEND,
        ProcessingToken default_graphics_token = ProcessingToken::GRAPHICS_BACKEND);

    ~TokenUnitManager() = default;

    // =========================================================================
    // Audio Unit Management
    // =========================================================================

    /**
     * @brief Gets or creates a root audio unit for the specified token
     * @param token Processing domain
     * @return Reference to the root audio unit
     *
     * If the unit doesn't exist, creates it and returns it.
     * Thread-safe creation via lock guard.
     */
    RootAudioUnit& get_or_create_audio_unit(ProcessingToken token);

    /**
     * @brief Gets an existing audio unit without creating if missing
     * @param token Processing domain
     * @return Const reference to the audio unit
     * @throws std::out_of_range if token not found
     *
     * This is a query-only operation; will not create a unit if missing.
     */
    const RootAudioUnit& get_audio_unit(ProcessingToken token) const;

    /**
     * @brief Gets an existing audio unit without creating if missing (mutable)
     * @param token Processing domain
     * @return Reference to the audio unit
     * @throws std::out_of_range if token not found
     *
     * This is a query-only operation; will not create a unit if missing.
     * Mutable version for operations that need to modify the unit in-place.
     */
    RootAudioUnit& get_audio_unit_mutable(ProcessingToken token);

    /**
     * @brief Ensures a root audio unit exists for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the root audio unit for that token/channel
     */
    RootAudioUnit& ensure_and_get_audio_unit(ProcessingToken token, uint32_t channel);

    /**
     * @brief Checks if an audio unit exists for the given token
     * @param token Processing domain
     * @return True if the unit exists, false otherwise
     */
    bool has_audio_unit(ProcessingToken token) const;

    /**
     * @brief Gets all active audio processing tokens
     * @return Vector of tokens that have audio units
     */
    std::vector<ProcessingToken> get_active_audio_tokens() const;

    // =========================================================================
    // Graphics Unit Management
    // =========================================================================

    /**
     * @brief Gets or creates a root graphics unit for the specified token
     * @param token Processing domain
     * @return Reference to the root graphics unit
     *
     * If the unit doesn't exist, creates it and initializes it.
     * Thread-safe creation via lock guard.
     */
    RootGraphicsUnit& get_or_create_graphics_unit(ProcessingToken token);

    /**
     * @brief Gets an existing graphics unit without creating if missing
     * @param token Processing domain
     * @return Const reference to the graphics unit
     * @throws std::out_of_range if token not found
     *
     * This is a query-only operation; will not create a unit if missing.
     */
    const RootGraphicsUnit& get_graphics_unit(ProcessingToken token) const;

    /**
     * @brief Gets an existing graphics unit without creating if missing (mutable)
     * @param token Processing domain
     * @return Reference to the graphics unit
     * @throws std::out_of_range if token not found
     *
     * This is a query-only operation; will not create a unit if missing.
     * Mutable version for operations that need to modify the unit in-place.
     */
    RootGraphicsUnit& get_graphics_unit_mutable(ProcessingToken token);

    /**
     * @brief Checks if a graphics unit exists for the given token
     * @param token Processing domain
     * @return True if the unit exists, false otherwise
     */
    bool has_graphics_unit(ProcessingToken token) const;

    /**
     * @brief Gets all active graphics processing tokens
     * @return Vector of tokens that have graphics units
     */
    std::vector<ProcessingToken> get_active_graphics_tokens() const;

    // =========================================================================
    // Audio Unit Operations
    // =========================================================================

    /**
     * @brief Ensures an audio unit exists and resizes it to the specified channel count
     * @param token Processing domain
     * @param channel_count Minimum channel count to ensure
     */
    void ensure_audio_channels(ProcessingToken token, uint32_t channel_count);

    /**
     * @brief Resizes all buffers in an audio unit to the specified size
     * @param token Processing domain
     * @param buffer_size New buffer size
     */
    void resize_audio_buffers(ProcessingToken token, uint32_t buffer_size);

    /**
     * @brief Gets the number of channels in an audio unit
     * @param token Processing domain
     * @return Number of channels, or 0 if unit doesn't exist
     */
    uint32_t get_audio_channel_count(ProcessingToken token) const;

    /**
     * @brief Gets the buffer size for an audio unit
     * @param token Processing domain
     * @return Buffer size, or 512 if unit doesn't exist
     */
    uint32_t get_audio_buffer_size(ProcessingToken token) const;

    /**
     * @brief Validates the number of channels and resizes buffers if necessary
     * @param token Processing domain
     * @param num_channels Number of channels to validate
     * @param buffer_size New buffer size to set
     *
     * This method ensures that the specified number of channels exists for the given token,
     * resizing the root audio buffers accordingly.
     */
    inline void validate_num_audio_channels(ProcessingToken token, uint32_t num_channels, uint32_t buffer_size)
    {
        ensure_audio_channels(token, num_channels);
        resize_audio_buffers(token, buffer_size);
    }

    // =========================================================================
    // Configuration Access
    // =========================================================================

    /**
     * @brief Gets the default processing token
     * @return The token configured at construction
     */
    inline ProcessingToken get_default_audio_token() const { return m_default_audio_token; }

    /**
     * @brief Gets the default graphics processing token
     * @return The graphics token configured at construction
     */
    inline ProcessingToken get_default_graphics_token() const { return m_default_graphics_token; }

    // =========================================================================
    // Thread Safety Access
    // =========================================================================

    /**
     * @brief Acquires the manager's mutex for external synchronization
     * @return Reference to the internal mutex
     *
     * Use this when you need to perform multiple atomic operations across
     * the unit store. Example:
     * @code
     * std::lock_guard<std::mutex> lock(manager.get_mutex());
     * auto& unit = manager.get_audio_unit_mutable(token);
     * // Multiple operations on unit are now atomic
     * @endcode
     */
    inline std::mutex& get_mutex() const { return m_manager_mutex; }

private:
    // =========================================================================
    // Data Members
    // =========================================================================

    /// Default processing token for legacy compatibility and initialization
    ProcessingToken m_default_audio_token;

    /// Default graphics processing token
    ProcessingToken m_default_graphics_token;

    /// Token-based map of root audio buffer units
    /// Maps: ProcessingToken -> channel -> {root_buffers, processing chains}
    std::unordered_map<ProcessingToken, RootAudioUnit> m_audio_units;

    /// Token-based map of root graphics buffer units
    std::unordered_map<ProcessingToken, RootGraphicsUnit> m_graphics_units;

    /// Mutex for thread-safe access to all unit maps
    mutable std::mutex m_manager_mutex;
};

}
