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
    class SoundContainerBuffer;
    class TextureBuffer;
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
 * @return Vector of shared pointers to created SoundContainerBuffer instances
 *
 * Establishes connection between loaded audio container and engine's buffer system,
 * enabling immediate audio playback through the standard processing pipeline.
 * Creates SoundContainerBuffer instances for each channel and connects to AUDIO_BACKEND token.
 * Multiple containers can be connected simultaneously for layered playback.
 */
MAYAFLUX_API std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> hook_sound_container_to_buffers(const std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer>& container);

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

/**
 * @brief Loads an image file into a TextureBuffer
 * @param filepath Path to the image file to load
 * @return Shared pointer to loaded TextureBuffer, or nullptr on failure
 *
 * Supports common image formats such as PNG, JPEG, BMP, TGA, PSD, GIF, HDR, PIC, and PNM.
 * Returns nullptr on failure with error details logged to stderr.
 */
MAYAFLUX_API std::shared_ptr<Buffers::TextureBuffer> load_image_file(const std::string& filepath);

/**
 * @brief Checks if the given file is an audio file based on its extension
 * @param filepath Path to the file to check
 * @return true if the file is recognized as an audio file, false otherwise
 */
MAYAFLUX_API bool is_audio(const std::filesystem::path& filepath);

/**
 * @brief Checks if the given file is an image file based on its extension
 * @param filepath Path to the file to check
 * @return true if the file is recognized as an image file, false otherwise
 */
MAYAFLUX_API bool is_image(const std::filesystem::path& filepath);

}
