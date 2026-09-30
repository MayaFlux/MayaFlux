#include "MayaFlux/Kriya/BufferPipeline.hpp"

#include "MayaFlux/Kriya/PipelineHelpers/PipelineBufferData.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Staging/AudioWriteProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

    Kakshya::DataVariant select_operation_data(
        std::unordered_map<BufferOperation*, Kakshya::DataVariant>& operation_data,
        BufferOperation& op)
    {
        Kakshya::DataVariant selected;
        if (operation_data.find(&op) != operation_data.end()) {
            selected = operation_data[&op];
        } else {
            for (auto& entry : operation_data) {
                selected = entry.second;
                break;
            }
        }
        return selected;
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
    auto input_data = select_operation_data(m_operation_data, op);

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
                if (candidate.get_type() != BufferOperation::OpType::CAPTURE
                    || !candidate.m_capture.get_buffer()) {
                    continue;
                }
                const auto buf = candidate.m_capture.get_buffer();
                if (std::holds_alternative<std::vector<double>>(transformed)
                    && std::get<std::vector<double>>(transformed).size() == buf->get_data().size()) {
                    detail::write_to_buffer(buf, transformed);
                }
                break;
            }
        }
    }
}

void BufferPipeline::process_route(BufferOperation& op)
{
    auto data_to_route = select_operation_data(m_operation_data, op);

    if (op.m_target_buffer) {
        if (!m_buffer_manager) {
            error<std::invalid_argument>(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                std::source_location::current(),
                "BufferPipeline has no BufferManager for ROUTE-to-buffer operation");
        }

        if (!op.m_attached_processor) {
            auto writer = std::make_shared<Buffers::AudioWriteProcessor>();
            m_buffer_manager->add_processor(writer, op.m_target_buffer,
                Buffers::ProcessingToken::AUDIO_BACKEND);
            op.m_attached_processor = writer;
        }

        std::static_pointer_cast<Buffers::AudioWriteProcessor>(op.m_attached_processor)
            ->set_data(data_to_route);

    } else if (op.m_target_container) {
        detail::write_to_container(op.m_target_container, data_to_route, op.m_target_channel);
    }
}

void BufferPipeline::process_load(BufferOperation& op)
{
    auto loaded_data = detail::read_from_container(op.m_source_container,
        op.m_start_frame,
        op.m_load_length);

    if (op.m_target_buffer) {
        detail::write_to_buffer(op.m_target_buffer, loaded_data);
    }

    m_operation_data[&op] = loaded_data;
}

void BufferPipeline::process_fuse(BufferOperation& op, uint64_t cycle)
{
    std::vector<Kakshya::DataVariant> fusion_inputs;

    for (auto& source_buffer : op.m_source_buffers) {
        bool should_process = op.m_capture.get_processing_control() == BufferCapture::ProcessingControl::ON_CAPTURE;
        auto buffer_data = detail::extract_buffer_data(source_buffer, should_process);
        fusion_inputs.push_back(buffer_data);
    }

    for (auto& source_container : op.m_source_containers) {
        auto container_data = detail::read_from_container(source_container, 0, 0);
        fusion_inputs.push_back(container_data);
    }

    if (op.m_fusion_function && !fusion_inputs.empty()) {
        auto fused_data = op.m_fusion_function(fusion_inputs, cycle);

        if (op.m_target_buffer) {
            detail::write_to_buffer(op.m_target_buffer, fused_data);
        } else if (op.m_target_container) {
            detail::write_to_container(op.m_target_container, fused_data, op.m_target_channel);
        }

        m_operation_data[&op] = fused_data;
    }
}

void BufferPipeline::process_dispatch(BufferOperation& op, uint64_t cycle)
{
    auto data_to_dispatch = select_operation_data(m_operation_data, op);

    if (op.m_dispatch_handler) {
        op.m_dispatch_handler(data_to_dispatch, cycle);
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
        op.m_attached_processor = m_buffer_manager->attach_quick_process(
            op.m_buffer_modifier,
            op.m_target_buffer, Buffers::ProcessingToken::AUDIO_BACKEND);
        if (m_max_cycles != 0 && op.is_streaming()) {
            op.m_modify_cycle_count = m_max_cycles - cycle;
        }
    }

    if (op.m_modify_cycle_count > 0 && cycle >= op.m_modify_cycle_count - 1) {
        if (op.m_attached_processor) {
            m_buffer_manager->remove_processor(
                op.m_attached_processor,
                op.m_target_buffer);
            op.m_attached_processor = nullptr;
        }
    }
}

}
