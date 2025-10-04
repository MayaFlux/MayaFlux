#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux {

namespace Core {
    struct SubsystemTokens;
}

/**
 * @enum Domain
 * @brief Unified domain enum combining all three ProcessingToken subsystems
 *
 * This enum provides a unified interface for defining processing domains by combining
 * tokens from Nodes, Buffers, and Vruta subsystems. Each domain represents a complete
 * processing configuration that spans all three subsystems.
 *
 * The enum uses bitfield composition to create unified domain identifiers that can
 * be decomposed back into their constituent subsystem tokens when needed.
 */
enum Domain : u_int64_t {
    // ===== CORE AUDIO DOMAINS =====

    /**
     * @brief Standard real-time audio processing domain
     *
     * Combines:
     * - Nodes::ProcessingToken::AUDIO_RATE
     * - Buffers::ProcessingToken::AUDIO_BACKEND
     * - Vruta::ProcessingToken::SAMPLE_ACCURATE
     */
    AUDIO = (static_cast<u_int64_t>(Nodes::ProcessingToken::AUDIO_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::AUDIO_BACKEND) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::SAMPLE_ACCURATE)),

    /**
     * @brief High-performance parallel audio processing domain
     *
     * Combines:
     * - Nodes::ProcessingToken::AUDIO_RATE
     * - Buffers::ProcessingToken::AUDIO_PARALLEL
     * - Vruta::ProcessingToken::SAMPLE_ACCURATE
     */
    AUDIO_PARALLEL = (static_cast<u_int64_t>(Nodes::ProcessingToken::AUDIO_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::AUDIO_PARALLEL) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::SAMPLE_ACCURATE)),

    // ===== VISUAL/GRAPHICS DOMAINS =====

    /**
     * @brief Standard real-time graphics processing domain
     *
     * Combines:
     * - Nodes::ProcessingToken::VISUAL_RATE
     * - Buffers::ProcessingToken::GRAPHICS_BACKEND
     * - Vruta::ProcessingToken::FRAME_ACCURATE
     */
    GRAPHICS = (static_cast<u_int64_t>(Nodes::ProcessingToken::VISUAL_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::GRAPHICS_BACKEND) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::FRAME_ACCURATE)),

    /**
     * @brief Multi-rate graphics processing for adaptive frame rates
     *
     * Combines:
     * - Nodes::ProcessingToken::VISUAL_RATE
     * - Buffers::ProcessingToken::GRAPHICS_BACKEND
     * - Vruta::ProcessingToken::MULTI_RATE
     */
    GRAPHICS_ADAPTIVE = (static_cast<u_int64_t>(Nodes::ProcessingToken::VISUAL_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::GRAPHICS_BACKEND) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::MULTI_RATE)),

    // ===== CUSTOM DOMAINS =====

    /**
     * @brief Custom processing domain with on-demand scheduling
     *
     * Combines:
     * - Nodes::ProcessingToken::CUSTOM_RATE
     * - Buffers::ProcessingToken::SAMPLE_RATE | CPU_PROCESS | SEQUENTIAL
     * - Vruta::ProcessingToken::ON_DEMAND
     */
    CUSTOM_ON_DEMAND = (static_cast<u_int64_t>(Nodes::ProcessingToken::CUSTOM_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::SAMPLE_RATE | Buffers::ProcessingToken::CPU_PROCESS | Buffers::ProcessingToken::SEQUENTIAL) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::ON_DEMAND)),

    /**
     * @brief Custom processing domain with flexible scheduling
     *
     * Combines:
     * - Nodes::ProcessingToken::CUSTOM_RATE
     * - Buffers::ProcessingToken::FRAME_RATE | GPU_PROCESS | PARALLEL
     * - Vruta::ProcessingToken::CUSTOM
     */
    CUSTOM_FLEXIBLE = (static_cast<u_int64_t>(Nodes::ProcessingToken::CUSTOM_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::FRAME_RATE | Buffers::ProcessingToken::GPU_PPOCESS | Buffers::ProcessingToken::PARALLEL) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::CUSTOM)),

    // ===== HYBRID DOMAINS =====

    /**
     * @brief Audio-visual synchronization domain
     *
     * Processes audio at sample rate but syncs with frame-accurate scheduling
     * Useful for multimedia applications requiring tight A/V sync
     */
    AUDIO_VISUAL_SYNC = (static_cast<u_int64_t>(Nodes::ProcessingToken::AUDIO_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::SAMPLE_RATE | Buffers::ProcessingToken::CPU_PROCESS | Buffers::ProcessingToken::SEQUENTIAL) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::FRAME_ACCURATE)),

    /**
     * @brief GPU-accelerated audio processing domain
     *
     * Routes audio through GPU for compute-intensive processing
     * while maintaining sample-accurate timing
     */
    AUDIO_GPU = (static_cast<u_int64_t>(Nodes::ProcessingToken::AUDIO_RATE) << 32) | (static_cast<u_int64_t>(Buffers::ProcessingToken::SAMPLE_RATE | Buffers::ProcessingToken::GPU_PPOCESS | Buffers::ProcessingToken::PARALLEL) << 16) | (static_cast<u_int64_t>(Vruta::ProcessingToken::MULTI_RATE)),

    /**
     * @brief Pure windowing domain (no rendering)
     *
     * Handles window lifecycle, input events, and frame timing coordination.
     * Does not perform any GPU rendering - that's handled by GRAPHICS domain.
     *
     * Use case: Headless window manager, input-only applications, or when
     * separating windowing from rendering into different subsystems.
     */
    WINDOWING = (static_cast<u_int64_t>(Nodes::ProcessingToken::VISUAL_RATE) << 32)
        | (static_cast<u_int64_t>(Buffers::ProcessingToken::WINDOW_EVENTS) << 16)
        | (static_cast<u_int64_t>(Vruta::ProcessingToken::FRAME_ACCURATE)),

    /**
     * @brief Input event processing domain
     *
     * Handles input events (keyboard, mouse, MIDI) asynchronously without
     * frame synchronization. Events are processed as they arrive, not locked
     * to vsync or audio callbacks.
     *
     * Use case: Input gesture recognition, MIDI controllers, or when input
     * latency must be minimized independently of rendering frame rate.
     */
    INPUT_EVENTS = (static_cast<u_int64_t>(Nodes::ProcessingToken::CUSTOM_RATE) << 32)
        | (static_cast<u_int64_t>(Buffers::ProcessingToken::WINDOW_EVENTS) << 16)
        | (static_cast<u_int64_t>(Vruta::ProcessingToken::EVENT_DRIVEN)),
};

/**
 * @brief Decomposes a Domain enum into its constituent ProcessingTokens
 * @param domain The unified domain to decompose
 * @return SubsystemTokens struct containing the individual tokens
 */
Core::SubsystemTokens decompose_domain(Domain domain);

/**
 * @brief Composes individual ProcessingTokens into a unified Domain
 * @param node_token Token for node processing
 * @param buffer_token Token for buffer processing
 * @param task_token Token for task scheduling
 * @return Unified Domain enum value
 */
inline Domain compose_domain(Nodes::ProcessingToken node_token,
    Buffers::ProcessingToken buffer_token,
    Vruta::ProcessingToken task_token)
{
    return static_cast<Domain>(
        (static_cast<u_int64_t>(node_token) << 32) | (static_cast<u_int64_t>(buffer_token) << 16) | (static_cast<u_int64_t>(task_token)));
}

/**
 * @brief Creates a custom domain from individual tokens with validation
 * @param node_token Token for node processing
 * @param buffer_token Token for buffer processing
 * @param task_token Token for task scheduling
 * @return Validated unified Domain enum value
 * @throws std::invalid_argument if tokens are incompatible
 */
Domain create_custom_domain(Nodes::ProcessingToken node_token,
    Buffers::ProcessingToken buffer_token,
    Vruta::ProcessingToken task_token);

/**
 * @brief Extracts node processing token from domain
 * @param domain The unified domain
 * @return Node processing token
 */
inline Nodes::ProcessingToken get_node_token(Domain domain)
{
    return static_cast<Nodes::ProcessingToken>((domain >> 32) & 0xFFFF);
}

/**
 * @brief Extracts buffer processing token from domain
 * @param domain The unified domain
 * @return Buffer processing token
 */
inline Buffers::ProcessingToken get_buffer_token(Domain domain)
{
    return static_cast<Buffers::ProcessingToken>((domain >> 16) & 0xFFFF);
}

/**
 * @brief Extracts task processing token from domain
 * @param domain The unified domain
 * @return Task processing token
 */
inline Vruta::ProcessingToken get_task_token(Domain domain)
{
    return static_cast<Vruta::ProcessingToken>(domain & 0xFFFF);
}

/**
 * @brief Checks if a domain is valid (all constituent tokens are compatible)
 * @param domain The domain to validate
 * @return true if domain is valid, false otherwise
 */
bool is_domain_valid(Domain domain);

/**
 * @brief Gets a human-readable string representation of a domain
 * @param domain The domain to stringify
 * @return String description of the domain
 */
std::string domain_to_string(Domain domain);

} // namespace MayaFlux
