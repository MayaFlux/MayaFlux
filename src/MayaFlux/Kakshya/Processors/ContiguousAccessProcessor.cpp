#include "ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

ContiguousAccessProcessor::ContiguousAccessProcessor()
    : m_prepared(false)
    , m_is_looping(false)
    , m_handle_interleaved(true)
    , m_data_interleaved(false)
    , m_sample_rate(0)
    , m_num_channels(0)
    , m_total_samples(0)
    , m_total_frames(0)
    , m_current_position(0)
    , m_frame_duration_ms(0.0)
{
}

void ContiguousAccessProcessor::on_attach(std::shared_ptr<SignalSourceContainer> signal_source)
{
    if (!signal_source) {
        return;
    }

    m_source_container_weak = signal_source;

    store_metadata(signal_source);

    if (m_data_interleaved) {
        deinterleave_data(signal_source);
    }

    validate_data(signal_source);

    m_prepared = true;

    signal_source->mark_ready_for_processing(m_prepared);

    std::cout << std::format("ContiguousAccessProcessor attached and prepared: {} channels, {} Hz, {} total samples",
        m_num_channels, m_sample_rate, m_total_samples)
              << std::endl;
}

void ContiguousAccessProcessor::store_metadata(std::shared_ptr<SignalSourceContainer> signal)
{
    m_sample_rate = signal->get_sample_rate();
    m_num_channels = signal->get_num_audio_channels();
    m_total_samples = signal->get_num_frames_total() * m_num_channels;
    m_total_frames = signal->get_num_frames_total();

    m_is_looping = signal->get_looping();
    m_current_position = signal->get_read_position();
    m_data_interleaved = signal->is_interleaved();

    if (m_sample_rate > 0) {
        m_frame_duration_ms = 1000.0 / m_sample_rate;
    }
}

void ContiguousAccessProcessor::deinterleave_data(std::shared_ptr<SignalSourceContainer> signal_source)
{
    if (m_num_channels == 0 || m_total_frames == 0) {
        return;
    }

    std::cout << "Deinterleaving data for " << m_num_channels << " channels, "
              << m_total_frames << " frames" << std::endl;

    try {
        signal_source->lock();

        const auto& interleaved_data = signal_source->get_raw_samples();

        // For interleaved data, we expect size = frames * channels
        u_int64_t expected_interleaved_size = m_total_frames * m_num_channels;

        std::cout << "Interleaved data size: " << interleaved_data.size()
                  << ", Expected: " << expected_interleaved_size << std::endl;

        // Check if the data is actually interleaved
        if (interleaved_data.size() != expected_interleaved_size) {
            // The data size doesn't match what we expect for interleaved data
            // It might be already deinterleaved or there's an error
            throw std::runtime_error(std::format(
                "Interleaved data size ({}) doesn't match expected size ({}) for {} frames and {} channels",
                interleaved_data.size(), expected_interleaved_size, m_total_frames, m_num_channels));
        }

        m_channel_data.resize(m_num_channels);
        for (u_int32_t ch = 0; ch < m_num_channels; ch++) {
            m_channel_data[ch].resize(m_total_frames);
        }

        for (u_int64_t frame = 0; frame < m_total_frames; frame++) {
            for (u_int32_t ch = 0; ch < m_num_channels; ch++) {
                m_channel_data[ch][frame] = interleaved_data[frame * m_num_channels + ch];
            }
        }

        signal_source->set_all_raw_samples(m_channel_data);
        m_data_interleaved = false;

        signal_source->unlock();

    } catch (const std::exception& e) {
        std::cerr << "Error during deinterleaving: " << e.what() << std::endl;
        signal_source->unlock();
        m_channel_data.clear();
        throw;
    }
}

void ContiguousAccessProcessor::validate_data(std::shared_ptr<SignalSourceContainer> signal_source)
{
    if (!m_data_interleaved && !m_channel_data.empty()) {
        for (u_int32_t ch = 0; ch < m_num_channels; ch++) {
            if (ch >= m_channel_data.size() || m_channel_data[ch].size() != m_total_frames) {
                throw std::runtime_error(std::format(
                    "Channel {} data size invalid: expected {} frames",
                    ch, m_total_frames));
            }
        }
    } else if (m_prepared) {
        try {
            signal_source->lock();
            double test_sample = signal_source->get_sample_at(0, 0);
            (void)test_sample;
            signal_source->unlock();
        } catch (const std::exception& e) {
            signal_source->unlock();
            throw std::runtime_error(std::format(
                "Failed to access interleaved data: {}", e.what()));
        }
    }

    if (m_num_channels == 0) {
        throw std::runtime_error("Buffer has no channels");
    }

    if (m_total_frames == 0) {
        std::cerr << "Warning: Buffer has no frames" << std::endl;
    }
}

void ContiguousAccessProcessor::on_detach(std::shared_ptr<SignalSourceContainer> buffer)
{
    m_source_container_weak.reset();
    m_channel_data.clear();

    m_prepared = false;
    m_sample_rate = 0;
    m_num_channels = 0;
    m_total_samples = 0;
    m_total_frames = 0;
    m_current_position = 0;
    m_is_looping = false;
    m_data_interleaved = false;
}

void ContiguousAccessProcessor::process(std::shared_ptr<SignalSourceContainer> signal_source)
{
    if (!m_prepared) {
        std::cerr << "ContiguousAccessProcessor::process called before preparation" << std::endl;
        return;
    }

    auto sample_buffer_ptr = m_source_container_weak.lock();
    if (!sample_buffer_ptr) {
        std::cerr << "SampleSourceBuffer has been destroyed" << std::endl;
        return;
    }

    const u_int32_t HARDWARE_BUFFER_SIZE = 512;

    m_current_position = signal_source->get_read_position();
    m_is_looping = signal_source->get_looping();

    auto& processed_data = signal_source->get_processed_data();

    if (processed_data.size() != m_num_channels) {
        processed_data.resize(m_num_channels);
    }

    for (auto& channel_data : processed_data) {
        if (channel_data.size() != HARDWARE_BUFFER_SIZE) {
            channel_data.resize(HARDWARE_BUFFER_SIZE);
        }
    }

    if (m_is_looping) {
        process_with_looping(signal_source, processed_data, HARDWARE_BUFFER_SIZE);
    } else {
        process_without_looping(signal_source, processed_data, HARDWARE_BUFFER_SIZE);
    }

    signal_source->set_read_position(m_current_position);
}

void ContiguousAccessProcessor::process_with_looping(
    std::shared_ptr<SignalSourceContainer> signal_source,
    std::vector<std::vector<double>>& processed_data,
    u_int32_t hardware_buffer_size)
{
    if (m_total_frames == 0)
        return;

    for (u_int32_t ch = 0; ch < m_num_channels && ch < processed_data.size(); ch++) {
        auto& channel_output = processed_data[ch];

        for (u_int32_t i = 0; i < hardware_buffer_size; i++) {
            u_int64_t pos = (m_current_position + i) % m_total_frames;

            if (!m_data_interleaved) {
                if (ch < m_channel_data.size() && pos < m_channel_data[ch].size()) {
                    channel_output[i] = m_channel_data[ch][pos];
                } else {
                    channel_output[i] = 0.0;
                }
            } else {
                u_int64_t source_idx = pos * m_num_channels + ch;
                const auto& raw_data = signal_source->get_raw_samples();
                if (source_idx < raw_data.size()) {
                    channel_output[i] = raw_data[source_idx];
                } else {
                    channel_output[i] = 0.0;
                }
            }
        }
    }

    m_current_position = (m_current_position + hardware_buffer_size) % m_total_frames;
}

void ContiguousAccessProcessor::process_without_looping(
    std::shared_ptr<SignalSourceContainer> signal_source,
    std::vector<std::vector<double>>& processed_data,
    u_int32_t hardware_buffer_size)
{
    if (m_total_frames == 0)
        return;

    u_int32_t available_frames = static_cast<u_int32_t>(
        std::min(static_cast<u_int64_t>(hardware_buffer_size),
            m_total_frames - m_current_position));

    for (u_int32_t ch = 0; ch < m_num_channels && ch < processed_data.size(); ch++) {
        auto& channel_output = processed_data[ch];

        for (u_int32_t i = 0; i < available_frames; i++) {
            u_int64_t pos = m_current_position + i;

            if (!m_data_interleaved) {
                if (ch < m_channel_data.size() && pos < m_channel_data[ch].size()) {
                    channel_output[i] = m_channel_data[ch][pos];
                } else {
                    channel_output[i] = 0.0;
                }
            } else {
                u_int64_t source_idx = pos * m_num_channels + ch;
                const auto& raw_data = signal_source->get_raw_samples();
                if (source_idx < raw_data.size()) {
                    channel_output[i] = raw_data[source_idx];
                } else {
                    channel_output[i] = 0.0;
                }
            }
        }

        for (u_int32_t i = available_frames; i < hardware_buffer_size; i++) {
            channel_output[i] = 0.0;
        }
    }

    m_current_position += available_frames;

    if (m_current_position >= m_total_frames) {
        signal_source->update_processing_state(ProcessingState::NEEDS_REMOVAL);
    }
}

bool ContiguousAccessProcessor::is_processing() const
{
    if (auto container = m_source_container_weak.lock()) {
        return container->get_processing_state() == ProcessingState::PROCESSING;
    }

    return false;
}
}
