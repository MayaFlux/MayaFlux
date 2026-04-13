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
    template <size_t N>
    class SamplingPipeline;
}

/**
 * @brief Construct a built SamplingPipeline from an audio file.
 *
 * Loads the file into a DynamicSoundStream via SoundFileReader::load_bounded,
 * constructs a SamplingPipeline<N> with engine globals (BufferManager,
 * TaskScheduler, buffer size), calls build(), and returns the result.
 *
 * The returned sampler is ready for play() and play_continuous() calls.
 * All N voice slots are pre-loaded with a full-stream slice; individual
 * slots can be reconfigured via load() or slice() before triggering.
 *
 * @tparam N    Maximum concurrent voices (default: 4). Must be 2, 4, or 8.
 * @param filepath Path to the audio file (any FFmpeg-supported format).
 * @param num_samples Number of samples to load from the file (default: 48000 * 5).
 * @param channel  Output channel index (default: 0).
 * @return Built SamplingPipeline, or nullptr if the file could not be loaded.
 *
 * @code
 * auto kick = MayaFlux::create_sampler("kick.wav");
 * kick->play(0);
 *
 * auto pad  = MayaFlux::create_sampler<8>("pad.wav", 1);
 * pad->slice(0).speed = 0.5;
 * pad->play_continuous(0);
 * @endcode
 */
template <size_t N = 4>
MAYAFLUX_API std::shared_ptr<Kriya::SamplingPipeline<N>> create_sampler(
    const std::string& filepath, uint32_t num_samples = 48000 * 5, bool truncate = true,
    uint32_t channel = 0);

extern template MAYAFLUX_API std::shared_ptr<Kriya::SamplingPipeline<2>>
create_sampler<2>(const std::string&, uint32_t, bool, uint32_t);
extern template MAYAFLUX_API std::shared_ptr<Kriya::SamplingPipeline<4>>
create_sampler<4>(const std::string&, uint32_t, bool, uint32_t);
extern template MAYAFLUX_API std::shared_ptr<Kriya::SamplingPipeline<8>>
create_sampler<8>(const std::string&, uint32_t, bool, uint32_t);

} // namespace MayaFlux
