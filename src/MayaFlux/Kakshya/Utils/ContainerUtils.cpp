#include "ContainerUtils.hpp"
#include "DataUtils.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "RegionUtils.hpp"

#include "MayaFlux/Kakshya/StreamContainer.hpp"

namespace MayaFlux::Kakshya {

std::unordered_map<std::string, std::any> extract_processing_state_info(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    std::unordered_map<std::string, std::any> state_info;

    state_info["processing_state"] = static_cast<int>(container->get_processing_state());
    state_info["is_ready"] = container->is_ready_for_processing();

    if (auto stream = std::dynamic_pointer_cast<StreamContainer>(container)) {
        state_info["read_position"] = stream->get_read_position();
        state_info["is_stream_container"] = true;
    } else {
        state_info["is_stream_container"] = false;
    }

    return state_info;
}

std::unordered_map<std::string, std::any> extract_processor_info(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    std::unordered_map<std::string, std::any> processor_info;

    if (auto processor = container->get_default_processor()) {
        processor_info["has_processor"] = true;
        processor_info["processor_processing"] = processor->is_processing();
    } else {
        processor_info["has_processor"] = false;
    }

    if (auto chain = container->get_processing_chain()) {
        processor_info["has_processing_chain"] = true;
        // TODO: Could add more chain information here
    } else {
        processor_info["has_processing_chain"] = false;
    }

    return processor_info;
}

bool transition_state(ProcessingState& current_state, ProcessingState new_state, std::function<void()> on_transition)
{
    static const std::unordered_map<ProcessingState, std::unordered_set<ProcessingState>> valid_transitions = {
        { ProcessingState::IDLE, { ProcessingState::READY, ProcessingState::ERROR } },
        { ProcessingState::READY, { ProcessingState::PROCESSING, ProcessingState::IDLE, ProcessingState::ERROR } },
        { ProcessingState::PROCESSING, { ProcessingState::PROCESSED, ProcessingState::ERROR } },
        { ProcessingState::PROCESSED, { ProcessingState::READY, ProcessingState::IDLE } },
        { ProcessingState::ERROR, { ProcessingState::IDLE } },
        { ProcessingState::NEEDS_REMOVAL, { ProcessingState::IDLE } }
    };
    auto it = valid_transitions.find(current_state);
    if (it != valid_transitions.end() && (it->second.count(new_state) != 0U)) {
        current_state = new_state;
        if (on_transition)
            on_transition();
        return true;
    }
    return false;
}

std::unordered_map<std::string, std::any> analyze_access_pattern(const Region& region,
    const std::shared_ptr<SignalSourceContainer>& container)
{
    std::unordered_map<std::string, std::any> analysis;

    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto dimensions = container->get_dimensions();
    const auto memory_layout = container->get_memory_layout();

    analysis["is_contiguous"] = is_region_access_contiguous(region, container);
    analysis["memory_layout"] = static_cast<int>(memory_layout);

    u_int64_t region_size = 1;
    for (size_t i = 0; i < region.start_coordinates.size() && i < region.end_coordinates.size(); ++i) {
        region_size *= (region.end_coordinates[i] - region.start_coordinates[i] + 1);
    }
    analysis["region_size"] = region_size;

    if (!dimensions.empty()) {
        auto strides = calculate_strides(dimensions);
        analysis["access_stride"] = strides[0]; // Primary dimension stride
    }

    return analysis;
}

DataVariant extract_channel_data(const std::shared_ptr<SignalSourceContainer>& container,
    u_int32_t channel_index)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto structure = container->get_structure();
    auto channel_count = structure.get_channel_count();

    if (channel_index >= channel_count) {
        throw std::out_of_range("Channel index out of range");
    }

    auto data = container->get_data();

    if (structure.organization == OrganizationStrategy::PLANAR) {
        if (channel_index >= data.size()) {
            throw std::out_of_range("Channel index out of range for planar data");
        }
        return data[channel_index];
    }

    std::vector<double> temp_storage;
    auto interleaved_span = extract_from_variant<double>(data[0], temp_storage);

    if (interleaved_span.empty()) {
        throw std::runtime_error("Failed to extract interleaved data");
    }

    if (interleaved_span.size() % channel_count != 0) {
        throw std::runtime_error("Interleaved data size is not a multiple of channel count");
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    std::vector<double> channel_data;
    for (size_t i = channel_index; i < interleaved_span.size(); i += channel_count) {
        channel_data.push_back(interleaved_span[i]);
    }
#else
    auto channel_view = interleaved_span
        | std::views::drop(channel_index)
        | std::views::stride(channel_count);

    std::vector<double> channel_data;
    std::ranges::copy(channel_view, std::back_inserter(channel_data));
#endif // MAYAFLUX_PLATFORM_MACOS

    return DataVariant { std::move(channel_data) };
}

std::pair<std::shared_ptr<SignalSourceContainer>, std::vector<DataDimension>>
validate_container_for_analysis(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container is null or has no data");
    }

    auto dimensions = container->get_dimensions();
    if (dimensions.empty()) {
        throw std::runtime_error("Container has no dimensions");
    }

    return std::make_pair(container, std::move(dimensions));
}

std::vector<std::span<double>> extract_numeric_data(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container is null or has no data");
    }

    auto container_data = container->get_data();

    return container_data
        | std::views::transform([](DataVariant& variant) -> std::span<double> {
              return convert_variant_to_double(variant);
          })
        | std::ranges::to<std::vector>();
}

void validate_numeric_data_for_analysis(const std::vector<double>& data,
    const std::string& operation_name,
    size_t min_size)
{
    if (data.empty()) {
        throw std::invalid_argument("Cannot perform " + operation_name + " on empty data");
    }

    if (data.size() < min_size) {
        throw std::invalid_argument(operation_name + " requires at least " + std::to_string(min_size) + " data points, got " + std::to_string(data.size()));
    }

    if (auto invalid_it = std::ranges::find_if_not(data, [](double val) {
            return std::isfinite(val);
        });
        invalid_it != data.end()) {

        const auto index = std::distance(data.begin(), invalid_it);
        throw std::invalid_argument(operation_name + " data contains NaN or infinite values at index " + std::to_string(index));
    }
}

}
