// NOLINTBEGIN
#pragma once
#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"

/**
 * @brief Configure engine preferences (sample rate, buffer size, graphics, logging, etc.)
 *
 * This function runs BEFORE the engine starts. Think of it like opening
 * Preferences in your DAW—set up the environment once, and these settings persist
 * for the entire session.
 *
 * **Audio Configuration:**
 * - sample_rate: 48000 Hz (pro), 44100 Hz (CD), 96000 Hz (studio quality)
 * - buffer_size: 128 (low latency), 512 (default), 1024 (high quality)
 * - output.channels: 1 (mono), 2 (stereo), >2 (surround)
 *
 * **Graphics Configuration:**
 * - target_frame_rate: 60, 120, 144 Hz
 * - requested_api: VULKAN (default), OPENGL, METAL, DIRECTX12
 * - windowing_backend: GLFW (default), SDL, NATIVE, NONE (offscreen)
 *
 * **Logging:**
 * - set_journal_severity() for verbosity
 * - sink_journal_to_console() or store_journal_entries() for output
 *
 * @example Low-latency music performance setup:
 * @code
 * auto& stream = MayaFlux::Config::get_global_stream_info();
 * stream.sample_rate = 48000;
 * stream.buffer_size = 128;
 * @endcode
 *
 * @example Studio recording setup:
 * @code
 * auto& stream = MayaFlux::Config::get_global_stream_info();
 * stream.sample_rate = 96000;
 * stream.input.enabled = true;
 * stream.input.channels = 2;
 * @endcode
 *
 * For comprehensive documentation and platform-specific settings, see: docs/settings.md
 *
 * If you don't configure anything, MayaFlux uses sensible defaults:
 * 48 kHz, stereo, 512 sample buffer, Vulkan graphics, 60 FPS.
 */
void settings()
{
    // Your MayaFlux configuration code goes here!
}

// ============================================================================
// GLOBAL SCOPE
// ============================================================================
// Anything you write in this file above compose() is just C++ you can use
// inside compose(). If you don't use it in compose(), it does nothing.
// (This is standard C++ behavior—nothing executes until the code runs.)
// ============================================================================

// Your global state, helper functions, etc.
// ...

void compose()
{
    // Your MayaFlux code goes here!
}
// NOLINTEND
