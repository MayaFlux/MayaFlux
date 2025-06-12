#include "ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Kakshya/StreamContainer.hpp"

namespace MayaFlux::Kakshya {

void ContiguousAccessProcessor::on_attach(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container) {
        return;
    }

    m_source_container_weak = container;

    try {
        store_metadata(container);
        validate_container(container);

        m_prepared = true;
        container->mark_ready_for_processing(true);

        std::cout << std::format("ContiguousAccessProcessor attached: {} dimensions, {} total elements",
            m_dimensions.size(), m_total_elements)
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Failed to attach processor: " << e.what() << std::endl;
        m_prepared = false;
        throw;
    }
}

void ContiguousAccessProcessor::store_metadata(std::shared_ptr<SignalSourceContainer> container)
{
    m_dimensions = container->get_dimensions();
    m_memory_layout = container->get_memory_layout();
    m_total_elements = container->get_total_elements();

    m_current_position.assign(m_dimensions.size(), 0);

    if (m_output_shape.empty() && !m_dimensions.empty()) {
        m_output_shape.resize(m_dimensions.size(), 1);

        for (size_t i = 0; i < m_dimensions.size(); ++i) {
            if (m_dimensions[i].role == DataDimension::Role::CHANNEL) {
                m_output_shape[i] = m_dimensions[i].size;
            }
        }
    }

    if (m_active_dimensions.empty()) {
        m_active_dimensions.resize(m_dimensions.size());
        std::iota(m_active_dimensions.begin(), m_active_dimensions.end(), 0);
    }

    if (auto stream = std::dynamic_pointer_cast<StreamContainer>(container)) {
        m_looping_enabled = stream->is_looping();
        m_loop_region = stream->get_loop_region();

        if (!m_dimensions.empty()) {
            m_current_position[0] = stream->get_read_position();
        }
    }
}

void ContiguousAccessProcessor::validate_container(std::shared_ptr<SignalSourceContainer> container)
{
    if (m_dimensions.empty()) {
        throw std::runtime_error("Container has no dimensions");
    }

    if (m_total_elements == 0) {
        std::cerr << "Warning: Container has no data elements" << std::endl;
    }

    if (m_output_shape.size() != m_dimensions.size()) {
        throw std::runtime_error(std::format(
            "Output shape dimensionality ({}) doesn't match container dimensions ({})",
            m_output_shape.size(), m_dimensions.size()));
    }

    for (size_t i = 0; i < m_dimensions.size(); ++i) {
        if (m_output_shape[i] == 0) {
            throw std::runtime_error(std::format(
                "Output shape[{}] is zero, which is invalid", i));
        }
        if (m_output_shape[i] > m_dimensions[i].size) {
            throw std::runtime_error(std::format(
                "Output shape[{}] = {} exceeds dimension size {}",
                i, m_output_shape[i], m_dimensions[i].size));
        }
    }
}

void ContiguousAccessProcessor::on_detach(std::shared_ptr<SignalSourceContainer> container)
{
    m_source_container_weak.reset();
    m_dimensions.clear();
    m_current_position.clear();
    m_prepared = false;
    m_total_elements = 0;
}

void ContiguousAccessProcessor::process(std::shared_ptr<SignalSourceContainer> container)
{
    if (!m_prepared) {
        std::cerr << "ContiguousAccessProcessor not prepared" << std::endl;
        return;
    }

    auto source_container = m_source_container_weak.lock();
    if (!source_container || source_container.get() != container.get()) {
        std::cerr << "Container mismatch or expired" << std::endl;
        return;
    }

    m_is_processing = true;
    m_last_process_time = std::chrono::steady_clock::now();

    try {
        Region output_region = calculate_output_region(m_current_position, m_output_shape);

        DataVariant& processed_data = container->get_processed_data();

        process_region(container, output_region, processed_data);

        if (m_auto_advance) {
            advance_read_position(m_current_position, m_output_shape);

            if (m_looping_enabled) {
                handle_looping(m_current_position);
            }

            if (auto stream = std::dynamic_pointer_cast<StreamContainer>(container)) {
                if (!m_dimensions.empty()) {
                    stream->set_read_position(m_current_position[0]);
                }
            }
        }

        container->update_processing_state(ProcessingState::PROCESSED);

    } catch (const std::exception& e) {
        std::cerr << "Error during processing: " << e.what() << std::endl;
        container->update_processing_state(ProcessingState::ERROR);
    }

    m_is_processing = false;
}

void ContiguousAccessProcessor::process_region(std::shared_ptr<SignalSourceContainer> container,
    const Region& region,
    DataVariant& output)
{
    DataVariant region_data = container->get_region_data(region);

    safe_copy_data_variant(region_data, output);
}

Region ContiguousAccessProcessor::calculate_output_region(
    const std::vector<u_int64_t>& current_pos,
    const std::vector<u_int64_t>& output_shape) const
{
    std::vector<u_int64_t> end;
    for (size_t i = 0; i < current_pos.size(); ++i) {
        end.push_back(current_pos[i] + output_shape[i] - 1);
    }
    return Region(current_pos, end);
}

void ContiguousAccessProcessor::advance_read_position(std::vector<u_int64_t>& position, const std::vector<u_int64_t>& shape)
{
    assert(position.size() == shape.size() && "advance_read_position: position and shape size mismatch");

    if (!position.empty()) {
        u_int64_t loop_start = 0;
        u_int64_t loop_end = m_dimensions[0].size;

        if (!m_loop_region.start_coordinates.empty()) {
            loop_start = m_loop_region.start_coordinates[0];
        }
        if (!m_loop_region.end_coordinates.empty()) {
            loop_end = m_loop_region.end_coordinates[0];
        }

        position[0] = advance_position(
            position[0],
            shape[0],
            m_dimensions[0].size,
            loop_start,
            loop_end,
            m_looping_enabled);
    }
    m_current_position = position;
}

void ContiguousAccessProcessor::handle_looping(std::vector<u_int64_t>& position)
{
    if (!m_looping_enabled || m_loop_region.start_coordinates.empty()) {
        return;
    }

    for (size_t i = 0; i < position.size(); ++i) {
        position[i] = wrap_position_with_loop(position[i], m_loop_region, i, m_looping_enabled);
    }
}

} // namespace MayaFlux::Kakshya
