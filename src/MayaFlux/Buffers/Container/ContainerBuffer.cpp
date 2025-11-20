#include "ContainerBuffer.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"

#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

namespace MayaFlux::Buffers {

ContainerToBufferAdapter::ContainerToBufferAdapter(std::shared_ptr<Kakshya::StreamContainer> container)
    : m_container(container)
{
    if (container) {
        auto structure = container->get_structure();
        m_num_channels = structure.get_channel_count();

        container->register_state_change_callback(
            [this](auto c, auto s) {
                this->on_container_state_change(c, s);
            });
    }
}

void ContainerToBufferAdapter::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    if (m_container->is_at_end()) {
        buffer->mark_for_removal();
        return;
    }

    try {
        auto state = m_container->get_processing_state();

        if (state == Kakshya::ProcessingState::NEEDS_REMOVAL) {
            if (m_update_flags) {
                buffer->mark_for_removal();
            }
            return;
        }

        if (state != Kakshya::ProcessingState::PROCESSED) {
            if (state == Kakshya::ProcessingState::READY) {
                if (m_container->try_acquire_processing_token(m_source_channel)) {
                    m_container->process_default();
                }
            }
        }

        auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
        auto& buffer_data = audio_buffer->get_data();
        uint32_t buffer_size = audio_buffer->get_num_samples();

        auto read_positions = m_container->get_read_position();
        uint64_t current_pos = (m_source_channel < read_positions.size())
            ? read_positions[m_source_channel]
            : 0;

        if (buffer_data.size() != buffer_size) {
            buffer_data.resize(buffer_size);
        }

        extract_channel_data(buffer_data);

        if (m_auto_advance) {
            m_container->update_read_position_for_channel(m_source_channel, current_pos + buffer_size);
        }

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

        m_container->mark_dimension_consumed(m_source_channel, m_reader_id);

        if (m_container->all_dimensions_consumed()) {
            m_container->update_processing_state(Kakshya::ProcessingState::READY);
            std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(m_container)->clear_all_consumption();
            m_container->reset_processing_token();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in ContainerToBufferAdapter::process: " << e.what() << '\n';
    }
}

void ContainerToBufferAdapter::extract_channel_data(std::span<double> output)
{
    if (!m_container) {
        std::ranges::fill(output, 0.0);
        return;
    }

    auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundStreamContainer>(m_container);
    if (!sound_container) {
        std::ranges::fill(output, 0.0);
        return;
    }

    auto& processed_data = sound_container->get_processed_data();
    if (processed_data.empty()) {
        std::ranges::fill(output, 0.0);
        return;
    }

    auto structure = sound_container->get_structure();

    if (structure.organization == Kakshya::OrganizationStrategy::INTERLEAVED) {
        thread_local std::vector<double> temp_storage;
        auto data_span = Kakshya::extract_from_variant<double>(processed_data[0], temp_storage);

        auto num_channels = structure.get_channel_count();
        auto samples_to_copy = std::min(static_cast<size_t>(output.size()),
            static_cast<size_t>(data_span.size() / num_channels));

        for (auto i : std::views::iota(0UZ, samples_to_copy)) {
            auto interleaved_idx = i * num_channels + m_source_channel;
            output[i] = (interleaved_idx < data_span.size()) ? data_span[interleaved_idx] : 0.0;
        }

        if (samples_to_copy < output.size()) {
            std::ranges::fill(output | std::views::drop(samples_to_copy), 0.0);
        }

    } else {
        if (m_source_channel >= processed_data.size()) {
            std::ranges::fill(output, 0.0);
            return;
        }

        thread_local std::vector<double> temp_storage;
        auto channel_data_span = Kakshya::extract_from_variant<double>(processed_data[m_source_channel], temp_storage);

        auto samples_to_copy = std::min(output.size(), channel_data_span.size());
        std::ranges::copy_n(channel_data_span.begin(), samples_to_copy, output.begin());

        if (samples_to_copy < output.size()) {
            std::ranges::fill(output | std::views::drop(samples_to_copy), 0.0);
        }
    }
}

void ContainerToBufferAdapter::on_attach(std::shared_ptr<Buffer> buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    m_reader_id = m_container->register_dimension_reader(m_source_channel);

    if (!m_container->is_ready_for_processing()) {
        throw std::runtime_error("Container not ready for processing");
    }

    try {
        auto& buffer_data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();
        uint32_t num_samples = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_num_samples();

        extract_channel_data(buffer_data);

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error pre-filling buffer: " << e.what() << '\n';
    }
}

void ContainerToBufferAdapter::on_detach(std::shared_ptr<Buffer> buffer)
{
    if (m_container) {
        m_container->unregister_state_change_callback();
        m_container->unregister_dimension_reader(m_source_channel);
    }
}

void ContainerToBufferAdapter::set_source_channel(uint32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index exceeds container channel count");
    }
    m_source_channel = channel_index;
}

void ContainerToBufferAdapter::set_container(std::shared_ptr<Kakshya::StreamContainer> container)
{
    if (m_container) {
        m_container->unregister_state_change_callback();
    }

    m_container = container;

    if (container) {
        auto structure = container->get_structure();
        m_num_channels = structure.get_channel_count();

        container->register_state_change_callback(
            [this](std::shared_ptr<Kakshya::SignalSourceContainer> c, Kakshya::ProcessingState s) {
                this->on_container_state_change(c, s);
            });
    }
}

void ContainerToBufferAdapter::on_container_state_change(
    std::shared_ptr<Kakshya::SignalSourceContainer> container,
    Kakshya::ProcessingState state)
{
    switch (state) {
    case Kakshya::ProcessingState::NEEDS_REMOVAL:
        break;

    case Kakshya::ProcessingState::ERROR:
        std::cerr << "Container entered error state" << '\n';
        break;

    default:
        break;
    }
}

ContainerBuffer::ContainerBuffer(uint32_t channel_id, uint32_t num_samples,
    std::shared_ptr<Kakshya::StreamContainer> container,
    uint32_t source_channel)
    : AudioBuffer(channel_id, num_samples)
    , m_container(container)
    , m_source_channel(source_channel)
{
    if (!m_container) {
        throw std::invalid_argument("ContainerBuffer: container must not be null");
    }

    m_pending_adapter = std::make_shared<ContainerToBufferAdapter>(m_container);
    std::dynamic_pointer_cast<ContainerToBufferAdapter>(m_pending_adapter)->set_source_channel(m_source_channel);

    setup_zero_copy_if_possible();
}

void ContainerBuffer::initialize()
{
    if (m_pending_adapter) {
        set_default_processor(m_pending_adapter);
        enforce_default_processing(true);
        m_pending_adapter.reset();
    }
}

void ContainerBuffer::set_container(std::shared_ptr<Kakshya::StreamContainer> container)
{
    m_container = container;

    if (auto adapter = std::dynamic_pointer_cast<ContainerToBufferAdapter>(m_default_processor)) {
        adapter->set_container(container);
    }

    setup_zero_copy_if_possible();
}

void ContainerBuffer::setup_zero_copy_if_possible()
{
    // Check if we can use zero-copy mode
    // This would be possible if:
    // 1. Container data is contiguous doubles
    // 2. Channel is deinterleaved (column-major for audio)
    // 3. Buffer size matches container frame size

    if (!m_container) {
        m_zero_copy_mode = false;
        return;
    }

    auto dimensions = m_container->get_dimensions();
    auto layout = m_container->get_memory_layout();

    // For now, disable zero-copy until we have direct memory access APIs
    m_zero_copy_mode = false;

    // TODO: Implement zero-copy when container provides direct memory access
}

std::shared_ptr<BufferProcessor> ContainerBuffer::create_default_processor()
{
    if (m_pending_adapter) {
        return m_pending_adapter;
    }

    auto adapter = std::make_shared<ContainerToBufferAdapter>(m_container);
    adapter->set_source_channel(m_source_channel);
    return adapter;
}

} // namespace MayaFlux::Buffers
