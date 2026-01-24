#include "SoundContainerBuffer.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"

#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

SoundStreamReader::SoundStreamReader(const std::shared_ptr<Kakshya::StreamContainer>& container)
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

void SoundStreamReader::processing_function(std::shared_ptr<Buffer> buffer)
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
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SoundStreamReader: Error during processing: {}", e.what());
    }
}

void SoundStreamReader::extract_channel_data(std::span<double> output)
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

void SoundStreamReader::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    m_reader_id = m_container->register_dimension_reader(m_source_channel);

    if (!m_container->is_ready_for_processing()) {
        error<std::runtime_error>(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            std::source_location::current(),
            "SoundStreamReader: Container not ready for processing");
    }

    try {
        auto& buffer_data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();
        uint32_t num_samples = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_num_samples();

        extract_channel_data(buffer_data);

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

    } catch (const std::exception& e) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SoundStreamReader: Error pre-filling buffer: {}", e.what());
    }
}

void SoundStreamReader::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_container) {
        m_container->unregister_state_change_callback();
        m_container->unregister_dimension_reader(m_source_channel);
    }
}

void SoundStreamReader::set_source_channel(uint32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        error<std::out_of_range>(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            std::source_location::current(),
            "SoundStreamReader: Channel index {} exceeds container channel count {}",
            channel_index, m_num_channels);
    }
    m_source_channel = channel_index;
}

void SoundStreamReader::set_container(const std::shared_ptr<Kakshya::StreamContainer>& container)
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

void SoundStreamReader::on_container_state_change(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& /*container*/,
    Kakshya::ProcessingState state)
{
    switch (state) {
    case Kakshya::ProcessingState::NEEDS_REMOVAL:
        break;

    case Kakshya::ProcessingState::ERROR:
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SoundStreamReader: Container entered ERROR state");
        break;

    default:
        break;
    }
}

SoundContainerBuffer::SoundContainerBuffer(uint32_t channel_id, uint32_t num_samples,
    const std::shared_ptr<Kakshya::StreamContainer>& container,
    uint32_t source_channel)
    : AudioBuffer(channel_id, num_samples)
    , m_container(container)
    , m_source_channel(source_channel)
{
    if (!m_container) {
        error<std::invalid_argument>(Journal::Component::Buffers, Journal::Context::Init,
            std::source_location::current(),
            "SoundContainerBuffer: container must not be null");
    }

    m_pending_adapter = std::make_shared<SoundStreamReader>(m_container);
    std::dynamic_pointer_cast<SoundStreamReader>(m_pending_adapter)->set_source_channel(m_source_channel);

    setup_zero_copy_if_possible();
}

void SoundContainerBuffer::initialize()
{
    if (m_pending_adapter) {
        set_default_processor(m_pending_adapter);
        enforce_default_processing(true);
        m_pending_adapter.reset();
    }
}

void SoundContainerBuffer::set_container(const std::shared_ptr<Kakshya::StreamContainer>& container)
{
    m_container = container;

    if (auto adapter = std::dynamic_pointer_cast<SoundStreamReader>(m_default_processor)) {
        adapter->set_container(container);
    }

    setup_zero_copy_if_possible();
}

void SoundContainerBuffer::setup_zero_copy_if_possible()
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

    m_zero_copy_mode = false;

    // TODO: Implement zero-copy when container provides direct memory access
}

std::shared_ptr<BufferProcessor> SoundContainerBuffer::create_default_processor()
{
    if (m_pending_adapter) {
        return m_pending_adapter;
    }

    auto adapter = std::make_shared<SoundStreamReader>(m_container);
    adapter->set_source_channel(m_source_channel);
    return adapter;
}

} // namespace MayaFlux::Buffers
