#include "MayaFlux/Kriya/BufferPipeline.hpp"

#include "MayaFlux/Kriya/PipelineHelpers/PipelineBufferData.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Staging/AudioWriteProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

    void append_capture_data(Kakshya::DataVariant& existing, const Kakshya::DataVariant& incoming)
    {
        auto& existing_data = std::get<std::vector<double>>(existing);
        const auto& new_data = std::get<std::vector<double>>(incoming);
        existing_data.insert(existing_data.end(), new_data.begin(), new_data.end());
    }

    void update_circular_capture(Kakshya::DataVariant& existing, const Kakshya::DataVariant& incoming, uint32_t circular_size)
    {
        auto& circular = std::get<std::vector<double>>(existing);
        const auto& new_data = std::get<std::vector<double>>(incoming);

        circular.insert(circular.end(), new_data.begin(), new_data.end());

        if (circular.size() > circular_size) {
            circular.erase(circular.begin(),
                circular.begin() + static_cast<int64_t>(circular.size() - circular_size));
        }
    }

    void update_windowed_capture(Kakshya::DataVariant& existing, const Kakshya::DataVariant& incoming,
        uint32_t window_size, uint32_t hop_size)
    {
        auto& windowed = std::get<std::vector<double>>(existing);
        const auto& new_data = std::get<std::vector<double>>(incoming);

        if (windowed.size() >= window_size) {
            if (hop_size >= windowed.size()) {
                windowed = std::get<std::vector<double>>(incoming);
            } else {
                windowed.erase(windowed.begin(),
                    windowed.begin() + hop_size);

                windowed.insert(windowed.end(), new_data.begin(), new_data.end());

                if (windowed.size() > window_size) {
                    size_t excess = windowed.size() - window_size;
                    windowed.erase(windowed.begin(),
                        windowed.begin() + excess);
                }
            }
        } else {
            windowed.insert(windowed.end(), new_data.begin(), new_data.end());

            if (windowed.size() > window_size) {
                size_t excess = windowed.size() - window_size;
                windowed.erase(windowed.begin(),
                    windowed.begin() + excess);
            }
        }
    }

}

void BufferPipeline::capture_operation(BufferOperation& op, uint64_t cycle)
{
    bool should_process = op.m_capture.get_processing_control() == BufferCapture::ProcessingControl::ON_CAPTURE;
    auto buffer_data = detail::extract_buffer_data(op.m_capture.get_buffer(), should_process);

    if (op.m_capture.m_data_ready_callback) {
        op.m_capture.m_data_ready_callback(buffer_data, cycle);
    }

    auto capture_mode = op.m_capture.get_mode();

    switch (capture_mode) {
    case BufferCapture::CaptureMode::TRANSIENT:
        m_operation_data[&op] = buffer_data;
        break;

    case BufferCapture::CaptureMode::ACCUMULATE: {
        auto it = m_operation_data.find(&op);
        if (it == m_operation_data.end()) {
            m_operation_data[&op] = buffer_data;
        } else {
            try {
                append_capture_data(it->second, buffer_data);

            } catch (const std::bad_variant_access& e) {
                MF_ERROR(Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    "Data type mismatch during ACCUMULATE capture: {}",
                    e.what());
                m_operation_data[&op] = buffer_data;
            }
        }
        break;
    }
    case BufferCapture::CaptureMode::CIRCULAR: {
        uint32_t circular_size = op.m_capture.get_circular_size();
        if (circular_size == 0) {
            circular_size = 4096;
        }

        auto it = m_operation_data.find(&op);
        if (it == m_operation_data.end()) {
            m_operation_data[&op] = buffer_data;
        } else {
            try {
                update_circular_capture(it->second, buffer_data, circular_size);

            } catch (const std::bad_variant_access& e) {
                MF_ERROR(Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    "Data type mismatch during CIRCULAR capture: {}",
                    e.what());
                m_operation_data[&op] = buffer_data;
            } catch (std::exception& e) {
                error_rethrow(Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    std::source_location::current(),
                    "Error during CIRCULAR capture: {}",
                    e.what());
                m_operation_data[&op] = buffer_data;
            }
        }
        break;
    }
    case BufferCapture::CaptureMode::WINDOWED: {
        uint32_t window_size = op.m_capture.get_window_size();
        float overlap_ratio = op.m_capture.get_overlap_ratio();

        if (window_size == 0) {
            window_size = 512;
        }

        auto hop_size = static_cast<uint32_t>((float)window_size * (1.0F - overlap_ratio));
        if (hop_size == 0)
            hop_size = 1;

        auto it = m_operation_data.find(&op);
        if (it == m_operation_data.end()) {
            m_operation_data[&op] = buffer_data;
        } else {
            try {
                update_windowed_capture(it->second, buffer_data, window_size, hop_size);

            } catch (const std::bad_variant_access& e) {
                MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
                    "Data type mismatch during WINDOWED capture: {}", e.what());
                m_operation_data[&op] = buffer_data;
            }
        }
        break;
    }

    case BufferCapture::CaptureMode::TRIGGERED: {
        if (op.m_capture.m_stop_condition && op.m_capture.m_stop_condition()) {
            m_operation_data[&op] = buffer_data;
        }
        break;
    }

    default:
        m_operation_data[&op] = buffer_data;
        break;
    }

    if (has_immediate_routing(op)) {
        auto current_it = std::ranges::find_if(m_operations,
            [&op](const BufferOperation& o) { return &o == &op; });

        if (current_it != m_operations.end()) {
            auto next_it = std::next(current_it);
            if (next_it != m_operations.end() && next_it->get_type() == BufferOperation::OpType::ROUTE) {

                if (next_it->m_target_buffer) {
                    if (!m_buffer_manager) {
                        error<std::invalid_argument>(Journal::Component::Kriya,
                            Journal::Context::CoroutineScheduling,
                            std::source_location::current(),
                            "BufferPipeline has no BufferManager for immediate ROUTE-to-buffer");
                    }

                    if (!next_it->m_attached_processor) {
                        auto writer = std::make_shared<Buffers::AudioWriteProcessor>();
                        m_buffer_manager->add_processor(writer, next_it->m_target_buffer,
                            Buffers::ProcessingToken::AUDIO_BACKEND);
                        next_it->m_attached_processor = writer;
                    }

                    std::static_pointer_cast<Buffers::AudioWriteProcessor>(next_it->m_attached_processor)
                        ->set_data(buffer_data);
                } else if (next_it->m_target_container) {
                    detail::write_to_container(next_it->m_target_container, buffer_data, next_it->m_target_channel);
                }

                size_t route_index = std::distance(m_operations.begin(), next_it);
                if (route_index < m_data_states.size()) {
                    m_data_states[route_index] = DataState::CONSUMED;
                }
            }
        }
    }
}

void BufferPipeline::reset_accumulated_data()
{
    for (auto& op : m_operations) {
        if (op.get_type() == BufferOperation::OpType::CAPTURE) {
            auto mode = op.m_capture.get_mode();
            if (mode == BufferCapture::CaptureMode::ACCUMULATE || mode == BufferCapture::CaptureMode::CIRCULAR || mode == BufferCapture::CaptureMode::WINDOWED) {
                m_operation_data.erase(&op);
            }
        }
    }
}

bool BufferPipeline::has_immediate_routing(const BufferOperation& op) const
{
    auto it = std::ranges::find_if(m_operations,
        [&op](const BufferOperation& o) { return &o == &op; });

    if (it == m_operations.end() || std::next(it) == m_operations.end()) {
        return false;
    }

    auto next_op = std::next(it);
    return next_op->get_type() == BufferOperation::OpType::ROUTE;
}

void BufferPipeline::cleanup_expired_data()
{
    for (size_t i = 0; i < m_data_states.size(); ++i) {
        if (m_data_states[i] == DataState::READY) {
            if (i < m_operations.size() && m_operations[i].get_type() == BufferOperation::OpType::CAPTURE && m_operations[i].m_capture.get_mode() == BufferCapture::CaptureMode::TRANSIENT) {

                if (m_operations[i].m_capture.m_data_expired_callback) {
                    auto it = m_operation_data.find(&m_operations[i]);
                    if (it != m_operation_data.end()) {
                        m_operations[i].m_capture.m_data_expired_callback(it->second, m_current_cycle);
                    }
                }

                m_data_states[i] = DataState::EXPIRED;
            } else {
                m_data_states[i] = DataState::CONSUMED;
            }
        }
    }

    auto it = m_operation_data.begin();
    while (it != m_operation_data.end()) {
        if (m_current_cycle > 2) {
            it = m_operation_data.erase(it);
        } else {
            ++it;
        }
    }
}

}
