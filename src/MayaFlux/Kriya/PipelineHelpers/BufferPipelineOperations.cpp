#include "MayaFlux/Kriya/BufferPipeline.hpp"

#include "MayaFlux/Kriya/PipelineHelpers/PipelineBufferData.hpp"
#include "MayaFlux/Kriya/PipelineHelpers/PipelineGraphicsData.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Staging/AudioWriteProcessor.hpp"
#include "MayaFlux/Buffers/Container/VideoContainerBuffer.hpp"
#include "MayaFlux/Buffers/Staging/DataWriteProcessor.hpp"
#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/DynamicVideoStream.hpp"

namespace MayaFlux::Kriya {

namespace {

    std::optional<Kakshya::DataVariant> select_operation_data(
        const std::unordered_map<BufferOperation*, Kakshya::DataVariant>& operation_data,
        BufferOperation& op,
        bool graphics_read_pending)
    {
        if (const auto own = operation_data.find(&op); own != operation_data.end()) {
            return own->second;
        }

        if (!operation_data.empty()) {
            return operation_data.begin()->second;
        }

        if (graphics_read_pending) {
            return std::nullopt;
        }

        return Kakshya::DataVariant {};
    }

    bool same_extent(const Kakshya::DataVariant& a, const Kakshya::DataVariant& b)
    {
        const auto size_of = [](const auto& values) { return values.size(); };
        return a.index() == b.index() && std::visit(size_of, a) == std::visit(size_of, b);
    }

}

void BufferPipeline::process_operation(BufferOperation& op, uint64_t cycle)
{
    try {
        switch (op.get_type()) {
        case BufferOperation::OpType::CAPTURE:
            capture_operation(op, cycle);
            break;

        case BufferOperation::OpType::TRANSFORM:
            process_transform(op, cycle);
            break;

        case BufferOperation::OpType::ROUTE:
            process_route(op);
            break;

        case BufferOperation::OpType::LOAD:
            process_load(op);
            break;

        case BufferOperation::OpType::FUSE:
            process_fuse(op, cycle);
            break;

        case BufferOperation::OpType::DISPATCH:
            process_dispatch(op, cycle);
            break;

        case BufferOperation::OpType::MODIFY:
            process_modify(op, cycle);
            break;

        case BufferOperation::OpType::CONDITION:
            break;

        default:
            MF_ERROR(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                "Unknown operation type in pipeline : {} : {}",
                Reflect::enum_to_string(op.get_type()), std::to_string(static_cast<int>(op.get_type())));
            break;
        }
    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Error processing operation in BufferPipeline: {}",
            e.what());
    }
}

void BufferPipeline::process_transform(BufferOperation& op, uint64_t cycle)
{
    auto selected = select_operation_data(m_operation_data, op, !m_readers.empty());
    if (!selected) {
        return;
    }
    auto input_data = std::move(*selected);

    if (op.m_transformer) {
        auto transformed = op.m_transformer(input_data, cycle);
        m_operation_data[&op] = transformed;

        const bool has_downstream_route = std::ranges::any_of(
            m_operations,
            [](const BufferOperation& o) {
                return o.get_type() == BufferOperation::OpType::ROUTE;
            });

        if (!has_downstream_route) {
            for (auto& candidate : std::ranges::reverse_view(m_operations)) {
                if (&candidate == &op)
                    continue;
                if (candidate.get_type() != BufferOperation::OpType::CAPTURE) {
                    continue;
                }

                if (const auto buf = candidate.m_capture.get_audio_buffer()) {
                    if (std::holds_alternative<std::vector<double>>(transformed)
                        && std::get<std::vector<double>>(transformed).size() == buf->get_data().size()) {
                        detail::write_to_buffer(buf, transformed);
                    }
                    break;
                }

                if (const auto target = candidate.m_capture.get_graphics_buffer()) {
                    const auto captured = m_operation_data.find(&candidate);
                    if (captured != m_operation_data.end()
                        && detail::accepts_raw_write(target)
                        && same_extent(captured->second, transformed)) {
                        queue_graphics_write(op, target, transformed);
                    }
                    break;
                }
            }
        }
    }
}

void BufferPipeline::process_route(BufferOperation& op)
{
    const auto selected = select_operation_data(m_operation_data, op, !m_readers.empty());
    if (!selected) {
        return;
    }
    const auto& data_to_route = *selected;

    if (op.m_target_audio_buffer) {
        if (!m_buffer_manager) {
            error<std::invalid_argument>(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                std::source_location::current(),
                "BufferPipeline has no BufferManager for ROUTE-to-buffer operation");
        }

        if (!op.m_attached_processor) {
            auto writer = std::make_shared<Buffers::AudioWriteProcessor>();
            m_buffer_manager->add_processor(writer, op.m_target_audio_buffer,
                Buffers::ProcessingToken::AUDIO_BACKEND);
            op.m_attached_processor = writer;
        }

        std::static_pointer_cast<Buffers::AudioWriteProcessor>(op.m_attached_processor)
            ->set_data(data_to_route);

    } else if (op.m_target_graphics_buffer) {
        queue_graphics_write(op, op.m_target_graphics_buffer, data_to_route);

    } else if (op.m_target_audio_stream) {
        detail::write_to_container(op.m_target_audio_stream, data_to_route, op.m_target_audio_channel);

    } else if (op.m_target_graphics_stream) {
        detail::write_to_container(op.m_target_graphics_stream, data_to_route);
    }
}

void BufferPipeline::process_load(BufferOperation& op)
{
    auto loaded_data = op.m_source_graphics_stream
        ? detail::read_from_container(op.m_source_graphics_stream, op.m_start_frame, op.m_load_length)
        : detail::read_from_container(op.m_source_audio_stream, op.m_start_frame, op.m_load_length);

    if (op.m_target_audio_buffer) {
        detail::write_to_buffer(op.m_target_audio_buffer, loaded_data);
    } else if (op.m_target_graphics_buffer) {
        queue_graphics_write(op, op.m_target_graphics_buffer, loaded_data);
    }

    m_operation_data[&op] = loaded_data;
}

void BufferPipeline::process_fuse(BufferOperation& op, uint64_t cycle)
{
    std::vector<Kakshya::DataVariant> fusion_inputs;

    for (auto& source_buffer : op.m_source_audio_buffers) {
        bool should_process = op.m_capture.get_processing_control() == BufferCapture::ProcessingControl::ON_CAPTURE;
        auto buffer_data = detail::extract_buffer_data(source_buffer, should_process);
        fusion_inputs.push_back(buffer_data);
    }

    for (auto& source_container : op.m_source_audio_streams) {
        auto container_data = detail::read_from_container(source_container, 0, 0);
        fusion_inputs.push_back(container_data);
    }

    bool graphics_ready = true;
    for (auto& source_buffer : op.m_source_graphics_buffers) {
        if (auto buffer_data = read_graphics_buffer(op, source_buffer)) {
            fusion_inputs.push_back(std::move(*buffer_data));
        } else {
            graphics_ready = false;
        }
    }

    if (!graphics_ready) {
        return;
    }

    for (auto& source_container : op.m_source_graphics_streams) {
        fusion_inputs.push_back(detail::read_from_container(source_container, 0, 0));
    }

    if (op.m_fusion_function && !fusion_inputs.empty()) {
        auto fused_data = op.m_fusion_function(fusion_inputs, cycle);

        if (op.m_target_audio_buffer) {
            detail::write_to_buffer(op.m_target_audio_buffer, fused_data);
        } else if (op.m_target_graphics_buffer) {
            queue_graphics_write(op, op.m_target_graphics_buffer, fused_data);
        } else if (op.m_target_audio_stream) {
            detail::write_to_container(op.m_target_audio_stream, fused_data, op.m_target_audio_channel);
        } else if (op.m_target_graphics_stream) {
            detail::write_to_container(op.m_target_graphics_stream, fused_data);
        }

        m_operation_data[&op] = fused_data;
    }
}

void BufferPipeline::process_dispatch(BufferOperation& op, uint64_t cycle)
{
    auto selected = select_operation_data(m_operation_data, op, !m_readers.empty());
    if (!selected) {
        return;
    }

    if (op.m_dispatch_handler) {
        op.m_dispatch_handler(*selected, cycle);
    }
}

void BufferPipeline::process_modify(BufferOperation& op, uint64_t cycle)
{
    if (!m_buffer_manager) {
        error<std::invalid_argument>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "BufferPipeline has no BufferManager for MODIFY operation");
    }

    if (!op.m_attached_processor) {
        if (op.m_target_graphics_buffer) {
            op.m_attached_processor = m_buffer_manager->attach_quick_process(
                op.m_graphics_buffer_modifier,
                op.m_target_graphics_buffer, op.get_token());
        } else {
            op.m_attached_processor = m_buffer_manager->attach_quick_process(
                op.m_audio_buffer_modifier,
                op.m_target_audio_buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
        }
        if (m_max_cycles != 0 && op.is_streaming()) {
            op.m_modify_cycle_count = m_max_cycles - cycle;
        }
    }

    if (op.m_modify_cycle_count > 0 && cycle >= op.m_modify_cycle_count - 1) {
        if (op.m_attached_processor) {
            if (op.m_target_graphics_buffer) {
                m_buffer_manager->remove_processor(
                    op.m_attached_processor,
                    op.m_target_graphics_buffer);
            } else {
                m_buffer_manager->remove_processor(
                    op.m_attached_processor,
                    op.m_target_audio_buffer);
            }
            op.m_attached_processor = nullptr;
        }
    }
}

void BufferPipeline::prepare_displays()
{
    for (auto& op : m_operations) {
        if (!op.m_render || op.m_display_attached || !op.m_target_graphics_stream) {
            continue;
        }

        if (!m_io_manager) {
            error<std::invalid_argument>(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                std::source_location::current(),
                "BufferPipeline has no IOManager to display a stream route");
        }

        auto display = m_io_manager->hook_video_container_to_buffer(op.m_target_graphics_stream);
        if (!display) {
            error<std::runtime_error>(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                std::source_location::current(),
                "Failed to hook the stream to a display buffer");
        }

        detail::ensure_rendering(display, *op.m_render);
        op.m_display_attached = true;
    }
}

void BufferPipeline::queue_graphics_write(BufferOperation& op, const std::shared_ptr<Buffers::VKBuffer>& target, const Kakshya::DataVariant& data)
{
    if (!m_buffer_manager) {
        error<std::invalid_argument>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "BufferPipeline has no BufferManager for graphics buffer write");
    }

    if (!detail::accepts_raw_write(target)) {
        error<std::invalid_argument>(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Graphics buffer regenerates its own storage; write its owner, source or image instead");
    }

    if (!op.m_attached_processor) {
        auto writer = std::make_shared<Buffers::DataWriteProcessor>();
        m_buffer_manager->add_processor(writer, target, Buffers::ProcessingToken::GRAPHICS_BACKEND);
        op.m_attached_processor = writer;
    }

    std::static_pointer_cast<Buffers::DataWriteProcessor>(op.m_attached_processor)->set_data(data);
}

}
