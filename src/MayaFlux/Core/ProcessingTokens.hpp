#pragma once

namespace MayaFlux::Vruta {

enum class ProcessingToken {
    SAMPLE_ACCURATE, ///< Coroutine is sample-accurate
    FRAME_ACCURATE, ///< Coroutine is frame-accurate
    /**
     * @brief Event-driven execution - process when events arrive
     *
     * Unlike FRAME_ACCURATE (which waits for vsync) or SAMPLE_ACCURATE
     * (which waits for audio callback), EVENT_DRIVEN coroutines resume
     * whenever their associated events fire. Used for input handling,
     * UI interactions, or any sporadic/asynchronous processing.
     */
    EVENT_DRIVEN,
    MULTI_RATE, ///< Coroutine can handle multiple sample rates. Picks the frame-accurate processing token by default
    ON_DEMAND, ///< Coroutine is executed on demand, not scheduled
    CUSTOM
};

/**
 * @enum DelayContext
 * @brief Discriminator for different temporal delay mechanisms
 *
 * Allows routines to specify which timing mechanism should trigger
 * their resumption, preventing cross-contamination between different
 * temporal domains within the same processing token.
 */
enum class DelayContext : uint8_t {
    NONE, ///< No active delay, resume immediately
    SAMPLE_BASED, ///< Sample-accurate delay (audio domain)
    BUFFER_BASED, ///< Buffer-cycle delay (audio hardware boundary)
    EVENT_BASED, ///< Event-driven delay (user events, etc.)
    AWAIT ///< Awaiter-induced delay (temporary suspension)
};
}

namespace MayaFlux::Nodes {
/**
 * @enum ProcessingToken
 * @brief Enumerates the different processing domains for nodes
 *
 * This enum defines the various processing rates or domains that nodes can operate in.
 * Each token represents a specific type of processing, such as audio rate, visual rate,
 * or custom processing rates. Nodes can be registered under these tokens to indicate
 * their intended processing behavior within a RootNode.
 */
enum class ProcessingToken {
    AUDIO_RATE, ///< Nodes that process at the audio sample rate
    VISUAL_RATE, ///< Nodes that process at the visual frame rate
    CUSTOM_RATE ///< Nodes that process at a custom-defined rate
};
}

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
enum ProcessingToken : uint32_t {
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
    SEQUENTIAL = 0x10,

    /**
     * @brief Processes operations in parallel when possible
     *
     * Enables concurrent execution of processing operations when they can be
     * safely parallelized. This maximizes throughput for operations that can
     * benefit from parallel execution without introducing race conditions.
     */
    PARALLEL = 0x20,

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
    AUDIO_PARALLEL = SAMPLE_RATE | GPU_PPOCESS | PARALLEL,

    /**
     * @brief Window event stream processing
     *
     * Processes window lifecycle events (resize, close, focus) and input events
     * (keyboard, mouse) at frame rate using CPU in sequential order. This is
     * distinct from graphics rendering - it handles the window container itself,
     * not its visual content.
     */
    WINDOW_EVENTS = FRAME_RATE | CPU_PROCESS | SEQUENTIAL,
};

}
