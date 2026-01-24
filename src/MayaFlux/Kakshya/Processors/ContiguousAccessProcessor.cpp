#include "ContiguousAccessProcessor.hpp"

#include "MayaFlux/Kakshya/StreamContainer.hpp"

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

void ContiguousAccessProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        return;
    }

    m_source_container_weak = container;

    try {
        store_metadata(container);
        validate();

        m_prepared = true;
        container->mark_ready_for_processing(true);

        MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "ContiguousAccessProcessor attached: {} layout, {} total elements, {} channels",
            (m_structure.organization == OrganizationStrategy::INTERLEAVED ? "interleaved" : "planar"),
            m_total_elements,
            m_structure.get_channel_count());

    } catch (const std::exception& e) {
        m_prepared = false;
        error_rethrow(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Failed to attach ContiguousAccessProcessor: {}",
            e.what());
    }
}

void ContiguousAccessProcessor::store_metadata(const std::shared_ptr<SignalSourceContainer>& container)
{
    m_structure = container->get_structure();
    m_total_elements = container->get_total_elements();

    if (m_current_position.empty()) {
        uint64_t num_channels = m_structure.get_channel_count();
        m_current_position.assign(num_channels, 0);
    }

    if (m_output_shape.empty()) {
        uint64_t num_frames = m_structure.get_samples_count_per_channel();
        uint64_t num_channels = m_structure.get_channel_count();

        m_output_shape = {
            std::min<uint64_t>(1024UL, num_frames),
            num_channels
        };
    }

    if (auto stream = std::dynamic_pointer_cast<StreamContainer>(container)) {
        m_looping_enabled = stream->is_looping();
        m_loop_region = stream->get_loop_region();

        auto stream_positions = stream->get_read_position();
        if (!stream_positions.empty()) {
            m_current_position = stream_positions;
        }
    }
}

void ContiguousAccessProcessor::validate()
{
    if (m_total_elements == 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "ContiguousAccessProcessor validation: Container has no data elements");
    }

    if (m_output_shape.size() != 2) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Audio output shape must be [frames, channels]");
    }

    uint64_t frames_requested = m_output_shape[0];
    uint64_t channels_requested = m_output_shape[1];
    uint64_t available_channels = m_structure.get_channel_count();

    if (frames_requested == 0 || channels_requested == 0) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Frame and channel counts cannot be zero");
    }

    if (frames_requested > m_structure.get_samples_count_per_channel()) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Requested {} frames exceeds available {} samples per channel",
            frames_requested,
            m_structure.get_samples_count_per_channel());
    }

    if (channels_requested > available_channels) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Requested {} channels exceeds available {} channels",
            channels_requested,
            available_channels);
    }

    if (m_current_position.size() != available_channels) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "Current position size {} doesn't match available channel count {}, resetting positions",
            m_current_position.size(),
            available_channels);
        m_current_position.resize(available_channels, 0);
    }

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "ContiguousAccessProcessor validated: memory layout {}, processing {}×{} blocks, current positions for {} channels",
        (m_structure.organization == OrganizationStrategy::INTERLEAVED ? "interleaved" : "planar"),
        frames_requested,
        channels_requested, m_current_position.size());
}

void ContiguousAccessProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>& /*container*/)
{
    m_source_container_weak.reset();
    m_current_position.clear();
    m_prepared = false;
    m_total_elements = 0;
}

void ContiguousAccessProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!m_prepared) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "ContiguousAccessProcessor not prepared for processing");
        return;
    }

    auto source_container = m_source_container_weak.lock();
    if (!source_container || source_container.get() != container.get()) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "ContiguousAccessProcessor: Source container mismatch or expired");
        return;
    }

    m_is_processing = true;
    m_last_process_time = std::chrono::steady_clock::now();

    try {
        uint64_t min_frame = *std::ranges::min_element(m_current_position);
        std::vector<uint64_t> region_coords = { min_frame, 0 };

        Region output_region = calculate_output_region(region_coords, m_output_shape);

        auto region_data = container->get_region_data(output_region);
        auto& processed_data_vector = container->get_processed_data();

        if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
            processed_data_vector.resize(1);
            if (!region_data.empty()) {
                safe_copy_data_variant(region_data[0], processed_data_vector[0]);
            }
        } else {
            uint64_t channels_to_process = std::min(m_output_shape[1], static_cast<uint64_t>(region_data.size()));
            processed_data_vector.resize(channels_to_process);

            for (size_t ch = 0; ch < channels_to_process; ++ch) {
                safe_copy_data_variant(region_data[ch], processed_data_vector[ch]);
            }
        }

        if (m_auto_advance) {
            uint64_t frames_to_advance = m_output_shape[0];

            m_current_position = advance_position(
                m_current_position,
                frames_to_advance,
                m_structure,
                m_looping_enabled,
                m_loop_region);

            if (auto stream = std::dynamic_pointer_cast<StreamContainer>(container)) {
                stream->set_read_position(m_current_position);
            }
        }

        container->update_processing_state(ProcessingState::PROCESSED);
    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "Error during ContiguousAccessProcessor processing: {}",
            e.what());
        container->update_processing_state(ProcessingState::ERROR);
    }

    m_is_processing = false;
}

void ContiguousAccessProcessor::set_output_size(const std::vector<uint64_t>& shape)
{
    m_output_shape = shape;
    if (auto container = m_source_container_weak.lock()) {
        store_metadata(container);
        validate();
    }
}

} // namespace MayaFlux::Kakshya
