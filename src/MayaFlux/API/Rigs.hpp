#pragma once

/**
 * @file API/Rigs.hpp
 * @brief Pre-assembled, purpose-built signal flow configurations.
 *
 * Each function in this file constructs, wires, and returns a fully built
 * pipeline object ready for immediate use. The caller receives a live rig
 * with no further assembly required: IO, buffering, scheduling, and output
 * routing are resolved internally using engine globals.
 *
 * These are convenience entry points. Direct construction of the underlying
 * types (SamplingPipeline, BufferPipeline, etc.) remains available for cases
 * that require non-default configuration before build().
 */

namespace MayaFlux {

namespace Kriya {
    class SamplingPipeline;
}

/**
 * @brief Construct a built SamplingPipeline from an audio file.
 *
 * Loads the file into a DynamicSoundStream via SoundFileReader::load_bounded,
 * constructs a SamplingPipeline with engine globals (BufferManager,
 * TaskScheduler, buffer size), calls build(), and returns the result.
 *
 * The returned sampler is ready for play() and play_continuous() calls.
 * Voice slots are allocated on demand via load().
 *
 * @param filepath    Path to the audio file (any FFmpeg-supported format).
 * @param num_samples Number of samples to load from the file (default: 48000 * 5).
 * @param truncate    Truncate stream to num_samples if true (default: true).
 * @param channel     Output channel index (default: 0).
 * @return Built SamplingPipeline, or nullptr if the file could not be loaded.
 *
 * @code
 * auto kick = MayaFlux::create_sampler("kick.wav");
 * kick->play(0, kick->slice_from_stream());
 *
 * auto pad = MayaFlux::create_sampler("pad.wav");
 * pad->load(0, pad->slice_from_stream()).speed = 0.5;
 * pad->play_continuous(0);
 * @endcode
 */
MAYAFLUX_API std::shared_ptr<Kriya::SamplingPipeline> create_sampler(
    const std::string& filepath, uint32_t num_samples = 48000 * 5, bool truncate = true,
    uint32_t channel = 0);

} // namespace MayaFlux
