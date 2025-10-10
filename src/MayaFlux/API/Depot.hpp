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

class CreationContext;

namespace IO {
    class SoundFileReader;
}

namespace Kakshya {
    class SoundFileContainer;
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
std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> load_audio_file(const std::string& filepath);

/**
 * @brief Connects a SoundFileContainer to the buffer system for immediate playback
 * @param container SoundFileContainer to connect to buffers
 *
 * Establishes connection between loaded audio container and engine's buffer system,
 * enabling immediate audio playback through the standard processing pipeline.
 * Creates ContainerBuffer instances for each channel and connects to AUDIO_BACKEND token.
 * Multiple containers can be connected simultaneously for layered playback.
 */
void hook_sound_container_to_buffers(std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container);

void hook_sound_container_to_buffers_with_context(
    std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container,
    const CreationContext& context);

/**
 * @brief Registers container context operations for the Creator proxy system
 *
 * Sets up integration between Creator proxy system and container loading functions,
 * enabling fluent API usage like `vega.read("audio.wav") | Audio`.
 * Registers load_audio_file() as default loader and enables context-aware buffer connection.
 * Must be called during engine initialization before using Creator proxy with audio files.
 */
void register_container_context_operations();

}
