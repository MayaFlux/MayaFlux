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

namespace Kakshya {
    class DynamicSoundStream;
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
 * @param max_dur_ms  Optional maximum duration to build the pipeline for (in milliseconds).
                      Defaults to 0 which is infinite (the pipeline will run until the sampler is destroyed).
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
    uint32_t channel = 0, uint64_t max_dur_ms = 0);

/**
 * @brief Construct a built SamplingPipeline from an existing DynamicSoundStream.
 *
 * Allows multiple SamplingPipeline instances to share a single loaded stream,
 * one per output channel, without re-reading the file. The stream is typically
 * obtained from a previously created sampler via get_stream(), or loaded
 * directly via get_io_manager()->load_audio_bounded().
 *
 * @code
 * auto ch0 = MayaFlux::create_sampler("res/audio.wav", 48000 * 5);
 * auto ch1 = MayaFlux::create_sampler_from_stream(ch0->get_stream(), 1);
 * auto ch2 = MayaFlux::create_sampler_from_stream(ch0->get_stream(), 2);
 * @endcode
 *
 * @param stream     Loaded DynamicSoundStream to share.
 * @param channel    Output channel index (default: 0).
 * @param max_dur_ms Optional maximum duration in milliseconds. 0 for infinite.
 * @return Built SamplingPipeline, or nullptr if stream is null.
 */
MAYAFLUX_API std::shared_ptr<Kriya::SamplingPipeline> create_sampler_from_stream(
    std::shared_ptr<Kakshya::DynamicSoundStream> stream,
    uint32_t channel = 0, uint64_t max_dur_ms = 0);

} // namespace MayaFlux
