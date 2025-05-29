#include "ContainerBuffer.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

namespace MayaFlux::Buffers {

ContainerToBufferAdapter::ContainerToBufferAdapter(std::shared_ptr<Kakshya::StreamContainer> container)
    : m_container(container)
{
    if (container) {
        analyze_container_dimensions();

        container->register_state_change_callback(
            [this](std::shared_ptr<Kakshya::SignalSourceContainer> c, Kakshya::ProcessingState s) {
                this->on_container_state_change(c, s);
            });
    }
}

void ContainerToBufferAdapter::analyze_container_dimensions()
{
    if (!m_container)
        return;

    auto dimensions = m_container->get_dimensions();

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == Kakshya::DataDimension::Role::TIME) {
            m_dim_info.time_dim = i;
        } else if (dimensions[i].role == Kakshya::DataDimension::Role::CHANNEL) {
            m_dim_info.channel_dim = i;
            m_dim_info.has_channels = true;
            m_dim_info.num_channels = dimensions[i].size;
        }
    }

    if (dimensions.empty() || dimensions[m_dim_info.time_dim].role != Kakshya::DataDimension::Role::TIME) {
        throw std::runtime_error("Container missing time dimension for audio processing");
    }
}

void ContainerToBufferAdapter::process(std::shared_ptr<AudioBuffer> buffer)
{
    if (!m_container || !buffer) {
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
                m_container->process_default();
            }
            return;
        }

        auto& buffer_data = buffer->get_data();
        u_int32_t buffer_size = buffer->get_num_samples();
        u_int64_t current_pos = m_container->get_read_position();

        if (buffer_data.size() != buffer_size) {
            buffer_data.resize(buffer_size);
        }

        extract_channel_data(buffer_data, current_pos, buffer_size);

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

        if (m_auto_advance) {
            m_container->advance_read_position(buffer_size);

            m_container->mark_dimension_consumed(m_dim_info.time_dim);
            if (m_dim_info.has_channels) {
                m_container->mark_dimension_consumed(m_dim_info.channel_dim);
            }

            if (m_container->all_dimensions_consumed()) {
                m_container->update_processing_state(Kakshya::ProcessingState::READY);
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in ContainerToBufferAdapter::process: " << e.what() << std::endl;
    }
}

void ContainerToBufferAdapter::extract_channel_data(std::span<double> output,
    u_int64_t start_frame,
    u_int64_t num_frames)
{
    if (!m_container) {
        std::fill(output.begin(), output.end(), 0.0);
        return;
    }

    auto dimensions = m_container->get_dimensions();
    if (m_dim_info.has_channels && m_source_channel >= m_dim_info.num_channels) {
        std::fill(output.begin(), output.end(), 0.0);
        return;
    }

    u_int64_t available_frames = dimensions[m_dim_info.time_dim].size;
    u_int64_t loop_start = 0, loop_end = available_frames;
    bool looping = m_container->is_looping();
    if (looping) {
        auto region = m_container->get_loop_region();
        loop_start = region.start_coordinates[0];
        loop_end = region.end_coordinates[0];
    }

    u_int64_t pos = start_frame;
    size_t filled = 0;
    while (filled < num_frames) {
        u_int64_t region_end = looping ? loop_end : available_frames;
        u_int64_t frames_left = (pos < region_end) ? (region_end - pos) : 0;
        u_int64_t to_copy = std::min<u_int64_t>(num_frames - filled, frames_left);

        if (to_copy == 0) {
            if (looping) {
                pos = loop_start;
                continue;
            } else {
                std::fill(output.begin() + filled, output.end(), 0.0);
                break;
            }
        }

        Kakshya::RegionPoint region = Kakshya::RegionPoint::audio_span(
            pos, pos + to_copy - 1,
            m_source_channel, m_source_channel);

        auto region_data = m_container->get_region_data(region);
        Kakshya::safe_copy_data_variant_to_span(region_data, output.subspan(filled, to_copy));

        filled += to_copy;
        pos += to_copy;
    }
}

void ContainerToBufferAdapter::on_attach(std::shared_ptr<AudioBuffer> buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    m_container->register_dimension_reader(m_dim_info.time_dim);
    if (m_dim_info.has_channels) {
        m_container->register_dimension_reader(m_dim_info.channel_dim);
    }

    if (!m_container->is_ready_for_processing()) {
        throw std::runtime_error("Container not ready for processing");
    }

    try {
        auto& buffer_data = buffer->get_data();
        u_int32_t num_samples = buffer->get_num_samples();
        u_int64_t position = m_container->get_read_position();

        extract_channel_data(buffer_data, position, num_samples);

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error pre-filling buffer: " << e.what() << std::endl;
    }
}

void ContainerToBufferAdapter::on_detach(std::shared_ptr<AudioBuffer> buffer)
{
    if (m_container) {
        m_container->unregister_state_change_callback();
        m_container->unregister_dimension_reader(m_dim_info.time_dim);
        if (m_dim_info.has_channels) {
            m_container->unregister_dimension_reader(m_dim_info.channel_dim);
        }
    }
}

void ContainerToBufferAdapter::set_source_channel(u_int32_t channel)
{
    if (m_dim_info.has_channels && channel >= m_dim_info.num_channels) {
        throw std::out_of_range("Channel index exceeds container channel count");
    }
    m_source_channel = channel;
}

void ContainerToBufferAdapter::set_container(std::shared_ptr<Kakshya::StreamContainer> container)
{
    if (m_container) {
        m_container->unregister_state_change_callback();
    }

    m_container = container;

    if (container) {
        analyze_container_dimensions();
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
        std::cerr << "Container entered error state" << std::endl;
        break;

    default:
        break;
    }
}

ContainerBuffer::ContainerBuffer(u_int32_t channel_id, u_int32_t num_samples,
    std::shared_ptr<Kakshya::StreamContainer> container,
    u_int32_t source_channel)
    : StandardAudioBuffer(channel_id, num_samples)
    , m_container(container)
    , m_source_channel(source_channel)
{
    if (!m_container) {
        throw std::invalid_argument("ContainerBuffer: container must not be null");
    }

    // Create adapter but don't attach yet (need shared_from_this)
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
