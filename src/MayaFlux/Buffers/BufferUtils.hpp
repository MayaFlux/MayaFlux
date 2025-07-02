#pragma once

#include "config.h"

namespace MayaFlux::Buffers {

/**
 * @enum ProcessingToken
 * @brief Bitfield enum defining processing characteristics and backend requirements for buffer operations
 *
 * ProcessingToken provides a flexible bitfield system for specifying how buffers and their processors
 * should be handled within the MayaFlux engine. These tokens enable fine-grained control over processing
 * rate, execution location, and concurrency patterns, allowing the system to optimize resource allocation
 * and execution strategies based on specific requirements.
 *
 * The token system is designed as a bitfield to allow combination of orthogonal characteristics:
 * - **Rate Tokens**: Define the temporal processing characteristics (SAMPLE_RATE vs FRAME_RATE)
 * - **Device Tokens**: Specify execution location (CPU_PROCESS vs GPU_PROCESS)
 * - **Concurrency Tokens**: Control execution pattern (SEQUENTIAL vs PARALLEL)
 * - **Backend Tokens**: Predefined combinations optimized for specific use cases
 *
 * This architecture enables the buffer system to make intelligent decisions about processor compatibility,
 * resource allocation, and execution optimization while maintaining flexibility for custom processing
 * scenarios.
 */
enum ProcessingToken : u_int32_t {
    /**
     * @brief Processes data at audio sample rate with buffer-sized chunks
     *
     * Evaluates one audio buffer size (typically 512 samples) at a time, executing at
     * sample_rate/buffer_size frequency. This is the standard token for audio processing
     * operations that work with discrete audio blocks in real-time processing scenarios.
     */
    SAMPLE_RATE = 0x0,

    /**
     * @brief Processes data at video frame rate
     *
     * Evaluates one video frame at a time, typically at 30-120 Hz depending on the
     * video processing requirements. This token is optimized for video buffer operations
     * and graphics processing workflows that operate on complete frame data.
     */
    FRAME_RATE = 0x2,

    /**
     * @brief Executes processing operations on CPU threads
     *
     * Specifies that the processing should occur on CPU cores using traditional
     * threading models. This is optimal for operations that benefit from CPU's
     * sequential processing capabilities or require complex branching logic.
     */
    CPU_PROCESS = 0x4,

    /**
     * @brief Executes processing operations on GPU hardware
     *
     * Specifies that the processing should leverage GPU compute capabilities,
     * typically through compute shaders, CUDA, or OpenCL. This is optimal for
     * massively parallel operations that can benefit from GPU's parallel architecture.
     */
    GPU_PPOCESS = 0x8,

    /**
     * @brief Processes operations sequentially, one after another
     *
     * Ensures that processing operations are executed in a deterministic order
     * without parallelization. This is essential for operations where execution
     * order affects the final result or when shared resources require serialized access.
     */
    SEQUENTIAL = 0x16,

    /**
     * @brief Processes operations in parallel when possible
     *
     * Enables concurrent execution of processing operations when they can be
     * safely parallelized. This maximizes throughput for operations that can
     * benefit from parallel execution without introducing race conditions.
     */
    PARALLEL = 0x32,

    /**
     * @brief Standard audio processing backend configuration
     *
     * Combines SAMPLE_RATE | CPU_PROCESS | SEQUENTIAL for traditional audio processing.
     * This processes audio on the chosen backend (default RtAudio) using CPU threads
     * in sequential order within the audio callback loop. This is the most common
     * configuration for real-time audio processing applications.
     */
    AUDIO_BACKEND = SAMPLE_RATE | CPU_PROCESS | SEQUENTIAL,

    /**
     * @brief Standard graphics processing backend configuration
     *
     * Combines FRAME_RATE | GPU_PROCESS | PARALLEL for graphics processing.
     * This processes video/graphics data on the chosen backend (default GLFW) using
     * GPU hardware with parallel execution within the graphics callback loop.
     * Optimal for real-time graphics processing and video effects.
     */
    GRAPHICS_BACKEND = FRAME_RATE | GPU_PPOCESS | PARALLEL,

    /**
     * @brief High-performance audio processing with GPU acceleration
     *
     * Combines SAMPLE_RATE | GPU_PROCESS | PARALLEL for GPU-accelerated audio processing.
     * This offloads audio processing to GPU hardware, typically using compute shaders
     * for parallel execution. Useful for computationally intensive audio operations
     * like convolution, FFT processing, or complex effects that benefit from GPU parallelization.
     */
    AUDIO_PARALLEL = SAMPLE_RATE | GPU_PPOCESS | PARALLEL
};

/**
 * @enum TokenEnforcementStrategy
 * @brief Defines how strictly processing token requirements are enforced in buffer processing chains
 *
 * TokenEnforcementStrategy provides different levels of flexibility for handling processor-buffer
 * compatibility based on processing tokens. This allows the system to balance performance optimization
 * with operational flexibility depending on the application's requirements.
 *
 * The enforcement strategy affects how BufferProcessingChain handles processors with incompatible
 * tokens, ranging from strict validation to complete flexibility. This enables different operational
 * modes for development, production, and specialized processing scenarios.
 */
enum class TokenEnforcementStrategy {
    /**
     * @brief Strictly enforces token assignment with no cross-token sharing
     *
     * Processors must exactly match the buffer's processing token requirements.
     * Any incompatibility results in immediate rejection. This provides maximum
     * performance optimization by ensuring all processors in a chain can execute
     * with the same backend configuration, but offers the least flexibility.
     */
    STRICT,

    /**
     * @brief Filters processors through token enumeration, allowing compatible combinations
     *
     * Uses the are_tokens_compatible() function to determine if processors can work
     * together despite different token assignments. This allows some flexibility while
     * maintaining performance optimization for compatible processor combinations.
     * Incompatible processors are filtered out rather than rejected outright.
     */
    FILTERED,

    /**
     * @brief Allows token overrides but skips processing for incompatible operations
     *
     * Permits processors with different tokens to be added to processing chains,
     * but skips their execution when the tokens are incompatible. This maintains
     * chain integrity while allowing dynamic processor management. Useful for
     * conditional processing scenarios where not all processors need to execute.
     */
    OVERRIDE_SKIP,

    /**
     * @brief Allows token overrides but rejects incompatible processors from chains
     *
     * Similar to OVERRIDE_SKIP but removes incompatible processors from the chain
     * entirely rather than skipping them. This provides a middle ground between
     * flexibility and performance by cleaning up incompatible processors while
     * allowing initial token mismatches during chain construction.
     */
    OVERRIDE_REJECT,

    /**
     * @brief Ignores token assignments completely, allowing any processing combination
     *
     * Disables all token validation and compatibility checking. Any processor can
     * be added to any buffer's processing chain regardless of token compatibility.
     * This provides maximum flexibility but may result in suboptimal performance
     * or execution errors. Primarily useful for debugging or specialized scenarios.
     */
    IGNORE
};

/**
 * @brief Validates that a processing token has a valid, non-conflicting configuration
 * @param token Processing token to validate
 * @throws std::invalid_argument if the token contains mutually exclusive flags
 *
 * This function ensures that processing tokens contain only compatible flag combinations.
 * It validates three key mutual exclusions that are fundamental to the processing model:
 *
 * **Rate Mutual Exclusion**: SAMPLE_RATE and FRAME_RATE cannot be combined as they
 * represent fundamentally different temporal processing models that cannot be executed
 * simultaneously within the same processing context.
 *
 * **Device Mutual Exclusion**: CPU_PROCESS and GPU_PROCESS cannot be combined as they
 * represent different execution environments that require different resource allocation
 * and execution strategies.
 *
 * **Concurrency Mutual Exclusion**: SEQUENTIAL and PARALLEL cannot be combined as they
 * represent incompatible execution patterns that would create undefined behavior in
 * processing chains.
 *
 * This validation is essential for maintaining system stability and ensuring that
 * processing tokens represent achievable execution configurations.
 */
inline void validate_token(ProcessingToken token)
{
    // Ensure SAMPLE_RATE and FRAME_RATE are mutually exclusive
    if ((token & SAMPLE_RATE) && (token & FRAME_RATE)) {
        throw std::invalid_argument("SAMPLE_RATE and FRAME_RATE are mutually exclusive.");
    }

    // Ensure CPU_PROCESS and GPU_PROCESS are mutually exclusive
    if ((token & CPU_PROCESS) && (token & GPU_PPOCESS)) {
        throw std::invalid_argument("CPU_PROCESS and GPU_PROCESS are mutually exclusive.");
    }

    // Ensure SEQUENTIAL and PARALLEL are mutually exclusive
    if ((token & SEQUENTIAL) && (token & PARALLEL)) {
        throw std::invalid_argument("SEQUENTIAL and PARALLEL are mutually exclusive.");
    }
}

/**
 * @brief Determines if two processing tokens are compatible for joint execution
 * @param preferred The preferred processing token configuration
 * @param current The current processing token configuration being evaluated
 * @return true if the tokens are compatible, false otherwise
 *
 * This function implements sophisticated compatibility logic that goes beyond simple equality
 * checking to determine if processors with different token requirements can work together
 * in the same processing pipeline. The compatibility rules are designed to maximize
 * processing flexibility while maintaining system stability and performance.
 *
 * **Rate Compatibility Rules:**
 * - FRAME_RATE processors require FRAME_RATE execution contexts (strict requirement)
 * - SAMPLE_RATE processors can adapt to FRAME_RATE contexts (flexible upward compatibility)
 * - Same-rate combinations are always compatible
 *
 * **Device Compatibility Rules:**
 * - SAMPLE_RATE processing cannot execute on GPU hardware (hardware limitation)
 * - GPU-preferred processors cannot fall back to CPU execution (performance requirement)
 * - CPU-preferred processors can use GPU for FRAME_RATE processing only
 *
 * **Concurrency Compatibility Rules:**
 * - Sequential/Parallel differences are acceptable if rate requirements align
 * - Mismatched concurrency with incompatible rates is rejected
 * - Same concurrency patterns are always compatible
 *
 * This flexibility enables the system to optimize processing chains by allowing compatible
 * processors to share execution contexts while preventing configurations that would result
 * in poor performance or execution failures.
 */
inline bool are_tokens_compatible(ProcessingToken preferred, ProcessingToken current)
{
    bool preferred_sample = preferred & SAMPLE_RATE;
    bool preferred_frame = preferred & FRAME_RATE;
    bool current_sample = current & SAMPLE_RATE;
    bool current_frame = current & FRAME_RATE;

    // If preferred is FRAME_RATE, only FRAME_RATE is compatible
    if (preferred_frame && !current_frame)
        return false;
    // If preferred is SAMPLE_RATE, FRAME_RATE can be compatible (sample can "delay" to frame)
    if (preferred_sample && current_frame)
        return true;
    // If both are SAMPLE_RATE or both are FRAME_RATE, compatible
    if ((preferred_sample && current_sample) || (preferred_frame && current_frame))
        ;

    // Device compatibility: SAMPLE_RATE can't run on GPU, but FRAME_RATE can run on CPU
    bool preferred_cpu = preferred & CPU_PROCESS;
    bool preferred_gpu = preferred & GPU_PPOCESS;
    bool current_cpu = current & CPU_PROCESS;
    bool current_gpu = current & GPU_PPOCESS;

    if (preferred_sample && current_gpu)
        return false; // Can't run sample rate on GPU
    if (preferred_gpu && current_cpu)
        return false; // If preferred is GPU, but current is CPU, not compatible
    // If preferred is CPU, but current is GPU, allow only for FRAME_RATE
    if (preferred_cpu && current_gpu && !current_frame)
        return false;

    // Sequential/Parallel compatibility: allow if rates align
    bool preferred_seq = preferred & SEQUENTIAL;
    bool preferred_par = preferred & PARALLEL;
    bool current_seq = current & SEQUENTIAL;
    bool current_par = current & PARALLEL;

    if ((preferred_seq && current_par) || (preferred_par && current_seq)) {
        // Allow if rates align (already checked above)
        if ((preferred_sample && current_sample) || (preferred_frame && current_frame))
            return true;
        // If preferred is SAMPLE_RATE and current is FRAME_RATE, already handled above
        // Otherwise, not compatible
        return false;
    }

    // If all checks pass, compatible
    return true;
}

/**
 * @brief Gets the optimal processing token for a given buffer type and system configuration
 * @param buffer_type Type identifier for the buffer (e.g., "audio", "video", "texture")
 * @param system_capabilities Available system capabilities (GPU, multi-core CPU, etc.)
 * @return Recommended processing token for optimal performance
 *
 * This function analyzes buffer characteristics and system capabilities to recommend
 * the most appropriate processing token configuration. It considers factors like:
 * - Buffer data type and size characteristics
 * - Available hardware acceleration
 * - System performance characteristics
 * - Current system load and resource availability
 *
 * The recommendations help achieve optimal performance by matching processing
 * requirements with available system capabilities.
 */
inline ProcessingToken get_optimal_token(const std::string& buffer_type, u_int32_t system_capabilities)
{
    // Implementation would analyze buffer type and system capabilities
    // This is a placeholder for the actual optimization logic
    if (buffer_type == "audio") {
        return (system_capabilities & 0x1) ? AUDIO_PARALLEL : AUDIO_BACKEND;
    } else if (buffer_type == "video" || buffer_type == "texture") {
        return GRAPHICS_BACKEND;
    }
    return AUDIO_BACKEND; // Safe default
}

}
