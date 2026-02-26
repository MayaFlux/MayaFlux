#include "FrameAccessProcessor.hpp"

#include "MayaFlux/Kakshya/StreamContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

// =========================================================================
// Lifecycle
// =========================================================================

void FrameAccessProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
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
            "FrameAccessProcessor attached: {}×{}×{} frames={}, frame_bytes={}, batch={}",
            m_width, m_height, m_channels, m_total_frames,
            m_frame_byte_size, m_frames_per_batch);

    } catch (const std::exception& e) {
        m_prepared = false;
        error_rethrow(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Failed to attach FrameAccessProcessor: {}",
            e.what());
    }
}

void FrameAccessProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>& /*container*/)
{
    m_source_container_weak.reset();
    m_prepared = false;
    m_total_frames = 0;
    m_frame_byte_size = 0;
    m_current_frame = 0;
}

// =========================================================================
// Metadata
// =========================================================================

void FrameAccessProcessor::store_metadata(const std::shared_ptr<SignalSourceContainer>& container)
{
    m_structure = container->get_structure();

    const auto& dims = m_structure.dimensions;

    m_total_frames = ContainerDataStructure::get_frame_count(dims);
    m_height = ContainerDataStructure::get_height(dims);
    m_width = ContainerDataStructure::get_width(dims);
    m_channels = ContainerDataStructure::get_channel_count(dims);

    m_frame_byte_size = m_width * m_height * m_channels;

    if (auto stream = std::dynamic_pointer_cast<StreamContainer>(container)) {
        m_looping_enabled = stream->is_looping();
        m_loop_region = stream->get_loop_region();

        const auto& stream_positions = stream->get_read_position();
        if (!stream_positions.empty()) {
            m_current_frame = stream_positions[0];
        }
    }
}

void FrameAccessProcessor::validate()
{
    if (m_structure.modality != DataModality::VIDEO_COLOR
        && m_structure.modality != DataModality::IMAGE_COLOR) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "FrameAccessProcessor requires VIDEO_COLOR or IMAGE_COLOR modality, got {}",
            static_cast<int>(m_structure.modality));
    }

    if (m_width == 0 || m_height == 0) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Frame dimensions cannot be zero ({}×{})",
            m_width, m_height);
    }

    if (m_channels == 0) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "Channel count cannot be zero");
    }

    if (m_total_frames == 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameAccessProcessor: container has zero frames");
    }

    if (m_frames_per_batch > m_total_frames && m_total_frames > 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameAccessProcessor: batch size {} exceeds total frames {}, clamping",
            m_frames_per_batch, m_total_frames);
        m_frames_per_batch = m_total_frames;
    }

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "FrameAccessProcessor validated: {}×{}×{}, {} total frames, batch {}",
        m_width, m_height, m_channels, m_total_frames, m_frames_per_batch);
}

// =========================================================================
// Processing
// =========================================================================

void FrameAccessProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!m_prepared) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameAccessProcessor not prepared for processing");
        return;
    }

    auto source_container = m_source_container_weak.lock();
    if (!source_container || source_container.get() != container.get()) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameAccessProcessor: source container mismatch or expired");
        return;
    }

    m_is_processing = true;
    m_last_process_time = std::chrono::steady_clock::now();

    try {
        uint64_t frames_to_extract = std::min(m_frames_per_batch,
            m_total_frames > m_current_frame ? m_total_frames - m_current_frame : 0UL);

        if (frames_to_extract == 0 && m_looping_enabled && m_total_frames > 0) {
            m_current_frame = 0;
            if (!m_loop_region.start_coordinates.empty()) {
                m_current_frame = m_loop_region.start_coordinates[0];
            }
            frames_to_extract = std::min(m_frames_per_batch, m_total_frames - m_current_frame);
        }

        if (frames_to_extract == 0) {
            m_is_processing = false;
            return;
        }

        const auto* raw = static_cast<const uint8_t*>(container->get_raw_data());
        if (!raw) {
            MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "FrameAccessProcessor: container has no raw data");
            m_is_processing = false;
            return;
        }

        uint64_t byte_offset = m_current_frame * m_frame_byte_size;
        uint64_t byte_count = frames_to_extract * m_frame_byte_size;

        auto& processed_data_vector = container->get_processed_data();
        processed_data_vector.resize(1);

        auto* dest = std::get_if<std::vector<uint8_t>>(&processed_data_vector[0]);
        if (!dest) {
            processed_data_vector[0] = std::vector<uint8_t>();
            dest = std::get_if<std::vector<uint8_t>>(&processed_data_vector[0]);
        }
        dest->resize(byte_count);
        std::memcpy(dest->data(), raw + byte_offset, byte_count);

        if (m_auto_advance) {
            advance_frame(frames_to_extract);
        }

    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameAccessProcessor::process failed: {}", e.what());
    }

    m_is_processing = false;
}

// =========================================================================
// Configuration
// =========================================================================

void FrameAccessProcessor::set_frames_per_batch(uint64_t count)
{
    if (count == 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameAccessProcessor: batch size cannot be zero, clamping to 1");
        count = 1;
    }
    m_frames_per_batch = count;
}

// =========================================================================
// Frame advancement
// =========================================================================

void FrameAccessProcessor::advance_frame(uint64_t frames_to_advance)
{
    uint64_t new_frame = m_current_frame + frames_to_advance;

    if (new_frame >= m_total_frames) {
        if (m_looping_enabled) {
            uint64_t loop_start = 0;
            uint64_t loop_end = m_total_frames - 1;

            if (!m_loop_region.start_coordinates.empty()) {
                loop_start = m_loop_region.start_coordinates[0];
            }
            if (!m_loop_region.end_coordinates.empty()) {
                loop_end = m_loop_region.end_coordinates[0];
            }

            if (loop_end > loop_start) {
                uint64_t loop_length = loop_end - loop_start + 1;
                uint64_t overflow = new_frame - m_total_frames;
                new_frame = loop_start + (overflow % loop_length);
            } else {
                new_frame = loop_start;
            }
        } else {
            new_frame = m_total_frames > 0 ? m_total_frames - 1 : 0;
        }
    }

    m_current_frame = new_frame;

    if (auto stream = std::dynamic_pointer_cast<StreamContainer>(m_source_container_weak.lock())) {
        stream->update_read_position_for_channel(0, m_current_frame);
    }
}

} // namespace MayaFlux::Kakshya
