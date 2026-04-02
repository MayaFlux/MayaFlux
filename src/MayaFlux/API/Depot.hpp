#pragma once

/**
 * @file API/Depot.hpp
 * @brief Audio file loading and container management API
 *
 * This header provides the public API for working with IOManager,
 * container creation, and file type checking within the MayaFlux engine.
 * It includes:
 * - `create_container<ContainerType>(args...)`: Template function to create signal source containers.
 * - `is_audio(filepath)`: Check if a file is an audio file based on its extension.
 * - `is_image(filepath)`: Check if a file is an image file based on its extension.
 * - `get_io_manager()`: Access the global IOManager instance for file loading and buffer management.
 */

namespace MayaFlux {

namespace IO {
    class IOManager;
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
 * @brief Constructs and initializes per-channel SoundContainerBuffers without registering them.
 * @param container Source container.
 * @return One buffer per channel, unregistered, ready for manual routing.
 */
MAYAFLUX_API std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>>
prepare_audio_buffers(const std::shared_ptr<Kakshya::SoundFileContainer>& container);

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

/**
 * @brief Retrieves the global IOManager instance for file loading and buffer management
 * @return Shared pointer to the IOManager instance
 *
 * Provides access to the central IOManager responsible for all file loading operations,
 * container management, and buffer integration. This is the primary interface for
 * performing file I/O tasks within the MayaFlux engine.
 */
MAYAFLUX_API std::shared_ptr<IO::IOManager> get_io_manager();

}
