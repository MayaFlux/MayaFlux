#pragma once

/**
 * @file API/Core.hpp
 * @brief Core engine lifecycle and configuration API
 *
 * This header provides the fundamental engine control and configuration
 * functions that form the foundation of the MayaFlux framework. All other
 * subsystems depend on the engine being properly initialized and configured.
 *
 * The Core API handles:
 * - Engine initialization with various stream configurations
 * - Engine lifecycle management (Start, Pause, Resume, End)
 * - Access to core engine context and configuration
 * - Global stream information queries
 *
 * This is typically the first API users interact with when setting up
 * a MayaFlux application, and other API modules depend on these functions
 * for accessing engine subsystems.
 */
namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    struct GlobalGraphicsConfig;
    class Engine;
}

//-------------------------------------------------------------------------
// Engine Management
//-------------------------------------------------------------------------

/**
 * @brief Checks if the default engine has been initialized
 * @return true if the engine is initialized, false otherwise
 */
bool is_engine_initialized();

/**
 * @brief Gets the default engine instance
 * @return Reference to the default Engine
 *
 * Creates the engine if it doesn't exist yet. This is the centrally managed
 * engine instance that all convenience functions in this namespace operate on.
 */
Core::Engine& get_context();

/**
 * @brief Replaces the default engine with a new instance
 * @param instance New engine instance to use as default
 *
 * Transfers state from the old engine to the new one if possible.
 *
 * @warning This function uses move semantics. After calling this function,
 * the engine instance passed as parameter will be left in a moved-from state
 * and should not be used further. This is intentional to avoid resource duplication
 * and ensure proper transfer of ownership. Users should be careful to not access
 * the moved-from instance after calling this function.
 *
 * This function is intended for advanced use cases where custom engine configuration
 * is required beyond what the standard initialization functions provide.
 */
void set_and_transfer_context(Core::Engine instance);

/**
 * @brief Initializes the default engine with specified parameters
 * @param sample_rate Audio sample rate in Hz
 * @param buffer_size Size of audio processing buffer in frames
 * @param num_out_channels Number of output channels
 * @param num_in_channels Number of input channels
 *
 * Convenience wrapper for Engine::Init() on the default engine.
 */
void Init(uint32_t sample_rate = 48000, uint32_t buffer_size = 512, uint32_t num_out_channels = 2, uint32_t num_in_channels = 0);

/**
 * @brief Initializes the default engine with specified stream info
 * @param stream_info Configuration for sample rate, buffer size, and channels
 *
 * Convenience wrapper for Engine::Init() on the default engine.
 */
void Init(Core::GlobalStreamInfo stream_info);

/**
 * @brief Initializes the default engine with specified stream and graphics info
 * @param stream_info Configuration for sample rate, buffer size, and channels
 * @param graphics_info Configuration for graphics/windowing backend
 *
 * Convenience wrapper for Engine::Init() on the default engine.
 */
void Init(Core::GlobalStreamInfo stream_info, Core::GlobalGraphicsConfig graphics_config);

/**
 * @brief Starts audio processing on the default engine
 *
 * Convenience wrapper for Engine::Start() on the default engine.
 */
void Start();

/**
 * @brief Pauses audio processing on the default engine
 *
 * Convenience wrapper for Engine::Pause() on the default engine.
 */
void Pause();

/**
 * @brief Resumes audio processing on the default engine
 *
 * Convenience wrapper for Engine::Resume() on the default engine.
 */
void Resume();

/**
 * @brief Stops and cleans up the default engine
 *
 * Convenience wrapper for Engine::End() on the default engine.
 */
void End();

}
