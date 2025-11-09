#pragma once

/**
 * @file API/Depot.hpp
 * @brief Audio file loading and container management API
 *
 * This header provides high-level functions for loading audio files and integrating
 * them with the MayaFlux buffer system. It handles file format detection, audio
 * data extraction, and automatic buffer setup for immediate playback or processing.
 */

namespace MayaFlux {

namespace IO {
    class SoundFileReader;
}

namespace Kakshya {
    class SoundFileContainer;
    class SignalSourceContainer;
}

namespace Buffers {
    class ContainerBuffer;
}

/**
 * @brief Loads an audio file into a SoundFileContainer with automatic format detection
 * @param filepath Path to the audio file to load
 * @return Shared pointer to loaded SoundFileContainer, or nullptr on failure
 *
 * Provides comprehensive audio file loading with format detection, sample rate conversion,
 * and bit depth optimization. Supports all FFmpeg formats including WAV, MP3, FLAC, OGG, etc.
 * The container is immediately ready for use with buffer system integration.
 * Returns nullptr on failure with error details logged to stderr.
 */
MAYAFLUX_API std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> load_audio_file(const std::string& filepath);

/**
 * @brief Connects a SoundFileContainer to the buffer system for immediate playback
 * @param container SoundFileContainer to connect to buffers
 * @return Vector of shared pointers to created ContainerBuffer instances
 *
 * Establishes connection between loaded audio container and engine's buffer system,
 * enabling immediate audio playback through the standard processing pipeline.
 * Creates ContainerBuffer instances for each channel and connects to AUDIO_BACKEND token.
 * Multiple containers can be connected simultaneously for layered playback.
 */
MAYAFLUX_API std::vector<std::shared_ptr<Buffers::ContainerBuffer>> hook_sound_container_to_buffers(const std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer>& container);

/**
 * @brief creates a new container of the specified type
 * @tparam ContainerType Type of container to create (must be derived from SignalSourceContainer)
 * @tparam Args Constructor argument types
 * @param args Constructor arguments for the container
 * @return Shared pointer to the created container
 */
template <typename ContainerType, typename... Args>
    requires std::derived_from<ContainerType, Kakshya::SignalSourceContainer>
auto create_container(Args&&... args) -> std::shared_ptr<ContainerType>
{
    return std::make_shared<ContainerType>(std::forward<Args>(args)...);
}

}
