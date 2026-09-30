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
    class TapSetBuilder;
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

/**
 * @brief Construct one built SamplingPipeline per channel from an audio file.
 *
 * Loads the file once into a shared DynamicSoundStream, then constructs one
 * SamplingPipeline per channel. All pipelines share the same stream with no
 * redundant IO. max_channels = 0 uses all channels available in the file.
 *
 * @code
 * auto ch = MayaFlux::create_samplers("res/stereo.wav", 48000 * 5);
 * ch[0]->play(0, ch[0]->slice_from_stream());
 * ch[1]->play(0, ch[1]->slice_from_stream());
 * @endcode
 *
 * @param filepath     Path to the audio file (any FFmpeg-supported format).
 * @param num_samples  Number of samples to load (default: 48000 * 5).
 * @param truncate     Truncate stream to num_samples if true (default: true).
 * @param max_dur_ms   Optional maximum duration in milliseconds. 0 for infinite.
 * @param max_channels Maximum channels to create pipelines for. 0 = all available.
 * @return Vector of built SamplingPipelines, one per channel. Empty on load failure.
 */
MAYAFLUX_API std::vector<std::shared_ptr<Kriya::SamplingPipeline>> create_samplers(
    const std::string& filepath, uint32_t num_samples = 48000 * 5, bool truncate = true,
    uint64_t max_dur_ms = 0, uint32_t max_channels = 0);

/**
 * @brief Begin describing several taps that read one audio file.
 *
 * Loads the file like create_sampler and returns a builder. Each tap() adds a
 * tap with its own entry time, speed, direction, level and channel, and
 * start() plays them. Keep the resulting TapSet alive for as long as it should
 * sound.
 *
 * @code
 * auto taps = MayaFlux::create_tap_set("res/h.wav")
 *     .tap().speed(1.0)
 *     .tap().enters_after(2.0).speed(3.0 / 2.0).level(0.8)
 *     .tap().enters_after(4.0).speed(2.0).backward().level(0.6)
 *     .start();
 * @endcode
 *
 * @param filepath    Path to the audio file (any FFmpeg-supported format).
 * @param num_samples Number of samples to load from the file (default: 48000 * 5).
 * @param truncate    Truncate stream to num_samples if true (default: true).
 * @return Builder. If the file cannot be loaded, start() returns an empty TapSet.
 */
MAYAFLUX_API Kriya::TapSetBuilder create_tap_set(
    const std::string& filepath, uint32_t num_samples = 48000 * 5, bool truncate = true);

/**
 * @brief Begin describing several taps that read an existing DynamicSoundStream.
 *
 * Like create_tap_set, but reads a stream already in memory, for example one
 * shared with a sampler or filled by a recording.
 *
 * @param stream Source stream.
 * @return Builder. A null stream makes start() return an empty TapSet.
 */
MAYAFLUX_API Kriya::TapSetBuilder create_tap_set_from_stream(
    std::shared_ptr<Kakshya::DynamicSoundStream> stream);

} // namespace MayaFlux
