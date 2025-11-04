#pragma once

namespace MayaFlux::Buffers {

class Buffer;
class VKBuffer;
class AudioBuffer;

/**
 * @brief Audio processing function - receives correctly-typed AudioBuffer
 *
 * User writes:
 * @code
 * auto processor = [](const std::shared_ptr<AudioBuffer>& buf) {
 *     auto& data = buf->get_data();
 *     for (auto& sample : data) sample *= 0.5;
 * };
 *
 * buffer_manager->attach_processor(processor, audio_buffer, AUDIO_BACKEND);
 * buffer_manager->attach_processor_to_channel(processor, AUDIO_BACKEND, 0);
 * buffer_manager->attach_processor_to_token(processor, AUDIO_BACKEND);
 * @endcode
 *
 * No casting needed - it's already AudioBuffer!
 */
using AudioProcessingFunction = std::function<void(const std::shared_ptr<AudioBuffer>&)>;

/**
 * @brief Graphics processing function - receives correctly-typed VKBuffer
 *
 * User writes:
 * @code
 * auto processor = [](const std::shared_ptr<VKBuffer>& buf) {
 *     // buf is already VKBuffer, no casting
 * };
 *
 * buffer_manager->attach_processor(processor, graphics_buffer, GRAPHICS_BACKEND);
 * buffer_manager->attach_processor_to_channel(processor, GRAPHICS_BACKEND, 0);
 * buffer_manager->attach_processor_to_token(processor, GRAPHICS_BACKEND);
 * @endcode
 *
 * No casting needed - it's already VKBuffer!
 */
using GraphicsProcessingFunction = std::function<void(const std::shared_ptr<VKBuffer>&)>;

// ============================================================================
// Variant Type for Unified Dispatcher
// ============================================================================

using BufferProcessingFunction = std::variant<AudioProcessingFunction, GraphicsProcessingFunction>;
}
