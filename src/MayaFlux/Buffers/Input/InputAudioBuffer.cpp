#include "InputAudioBuffer.hpp"

namespace MayaFlux::Buffers {

InputAudioBuffer::InputAudioBuffer(u_int32_t channel_id, u_int32_t num_samples)
    : AudioBuffer(channel_id, num_samples)
{
}

void InputAudioBuffer::write_to(std::shared_ptr<AudioBuffer> buffer)
{
    if (!buffer) {
        std::cerr << "InputAudioBuffer: Attempted to write to null buffer" << std::endl;
        return;
    }

    const auto& src_data = get_data();
    auto& dst_data = buffer->get_data();

    if (dst_data.size() != src_data.size()) {
        dst_data.resize(src_data.size());
    }

    std::copy(src_data.begin(), src_data.end(), dst_data.begin());
}

void InputAudioBuffer::register_listener(std::shared_ptr<AudioBuffer> buffer)
{
    if (!buffer) {
        std::cerr << "InputAudioBuffer: Attempted to register null listener" << std::endl;
        return;
    }

    if (auto processor = std::dynamic_pointer_cast<InputAccessProcessor>(get_default_processor())) {
        processor->add_listener(buffer);
    }
}

void InputAudioBuffer::unregister_listener(std::shared_ptr<AudioBuffer> buffer)
{
    if (auto processor = std::dynamic_pointer_cast<InputAccessProcessor>(get_default_processor())) {
        processor->remove_listener(buffer);
    }
}

void InputAudioBuffer::clear_listeners()
{
    if (auto processor = std::dynamic_pointer_cast<InputAccessProcessor>(get_default_processor())) {
        processor->clear_listeners();
    }
}

void InputAccessProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto input_buffer = std::dynamic_pointer_cast<InputAudioBuffer>(buffer);
    if (!input_buffer)
        return;

    for (auto& listener : m_listeners) {
        if (!listener)
            continue;

        input_buffer->write_to(listener);
    }
}

void InputAccessProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    auto input_buffer = std::dynamic_pointer_cast<InputAudioBuffer>(buffer);
    if (!input_buffer) {
        throw std::runtime_error("InputAccessProcessor can only be attached to InputAudioBuffer");
    }
}

bool InputAccessProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
{
    return std::dynamic_pointer_cast<InputAudioBuffer>(buffer) != nullptr;
}

void InputAccessProcessor::add_listener(std::shared_ptr<AudioBuffer> buffer)
{
    if (!buffer)
        return;

    auto it = std::find(m_listeners.begin(), m_listeners.end(), buffer);
    if (it == m_listeners.end()) {
        m_listeners.push_back(buffer);
    }
}

void InputAccessProcessor::remove_listener(std::shared_ptr<AudioBuffer> buffer)
{
    auto it = std::find(m_listeners.begin(), m_listeners.end(), buffer);
    if (it != m_listeners.end()) {
        m_listeners.erase(it);
    }
}

}
