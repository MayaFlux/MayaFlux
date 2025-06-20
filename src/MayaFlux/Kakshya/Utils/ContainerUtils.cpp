#include "ContainerUtils.hpp"
// #include "DataUtils.hpp"
#include "DataUtils.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "RegionUtils.hpp"

namespace MayaFlux::Kakshya {

std::unordered_map<std::string, std::any> extract_processing_state_info(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    std::unordered_map<std::string, std::any> state_info;

    state_info["processing_state"] = static_cast<int>(container->get_processing_state());
    state_info["is_ready"] = container->is_ready_for_processing();

    // Try to get read position if it's a stream container
    if (auto stream = std::dynamic_pointer_cast<StreamContainer>(container)) {
        state_info["read_position"] = stream->get_read_position();
        state_info["is_stream_container"] = true;
    } else {
        state_info["is_stream_container"] = false;
    }

    return state_info;
}

std::unordered_map<std::string, std::any> extract_processor_info(std::shared_ptr<SignalSourceContainer> container)
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
        // Could add more chain information here
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
    if (it != valid_transitions.end() && it->second.count(new_state)) {
        current_state = new_state;
        if (on_transition)
            on_transition();
        return true;
    }
    return false;
}

std::unordered_map<std::string, std::any> analyze_access_pattern(const Region& region,
    std::shared_ptr<SignalSourceContainer> container)
{
    std::unordered_map<std::string, std::any> analysis;

    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto dimensions = container->get_dimensions();
    const auto memory_layout = container->get_memory_layout();

    analysis["is_contiguous"] = is_region_access_contiguous(region, container);
    analysis["memory_layout"] = static_cast<int>(memory_layout);

    // Calculate expected cache performance
    u_int64_t region_size = 1;
    for (size_t i = 0; i < region.start_coordinates.size() && i < region.end_coordinates.size(); ++i) {
        region_size *= (region.end_coordinates[i] - region.start_coordinates[i] + 1);
    }
    analysis["region_size"] = region_size;

    // Estimate memory stride
    if (!dimensions.empty()) {
        auto strides = calculate_strides(dimensions);
        analysis["access_stride"] = strides[0]; // Primary dimension stride
    }

    return analysis;
}

DataVariant extract_interleaved_data(std::shared_ptr<SignalSourceContainer> container,
    const std::vector<size_t>& pattern)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    // Default pattern: interleave all channels
    const auto dimensions = container->get_dimensions();
    std::vector<size_t> interleave_pattern = pattern;

    if (interleave_pattern.empty() && dimensions.size() >= 2) {
        // Find channel dimension
        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (dimensions[i].role == DataDimension::Role::CHANNEL) {
                for (size_t ch = 0; ch < dimensions[i].size; ++ch) {
                    interleave_pattern.push_back(ch);
                }
                break;
            }
        }
    }

    if (interleave_pattern.empty()) {
        return container->get_processed_data(); // No interleaving needed
    }

    // Extract data for each channel and interleave
    std::vector<DataVariant> channel_data;
    for (size_t ch_idx : interleave_pattern) {
        channel_data.push_back(extract_channel_data(container, static_cast<u_int32_t>(ch_idx)));
    }

    // Interleave the channel data
    return std::visit([&channel_data](const auto& first_channel) -> DataVariant {
        using T = typename std::decay_t<decltype(first_channel)>::value_type;
        std::vector<T> interleaved;

        // Assume all channels have same length
        size_t samples_per_channel = first_channel.size();
        interleaved.reserve(samples_per_channel * channel_data.size());

        for (size_t sample = 0; sample < samples_per_channel; ++sample) {
            for (size_t ch = 0; ch < channel_data.size(); ++ch) {
                const auto& ch_data = std::get<std::vector<T>>(channel_data[ch]);
                if (sample < ch_data.size()) {
                    interleaved.push_back(ch_data[sample]);
                }
            }
        }

        return DataVariant { interleaved };
    },
        channel_data[0]);
}

DataVariant extract_channel_data(std::shared_ptr<SignalSourceContainer> container,
    u_int32_t channel_index)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto dimensions = container->get_dimensions();
    if (dimensions.size() < 2) {
        throw std::invalid_argument("Container must have at least 2 dimensions for channel extraction");
    }

    // Find channel dimension
    size_t channel_dim_index = 0;
    bool found_channel_dim = false;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == DataDimension::Role::CHANNEL) {
            channel_dim_index = i;
            found_channel_dim = true;
            break;
        }
    }

    if (!found_channel_dim && dimensions.size() >= 2) {
        channel_dim_index = 1; // Default to second dimension
    }

    if (channel_index >= dimensions[channel_dim_index].size) {
        throw std::out_of_range("Channel index out of range");
    }

    // Create region for entire channel
    std::vector<u_int64_t> start_coords(dimensions.size(), 0);
    std::vector<u_int64_t> end_coords;
    end_coords.reserve(dimensions.size());

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (i == channel_dim_index) {
            start_coords[i] = channel_index;
            end_coords.push_back(channel_index);
        } else {
            end_coords.push_back(dimensions[i].size - 1);
        }
    }

    Region channel_region(start_coords, end_coords);
    return container->get_region_data(channel_region);
}

std::pair<std::shared_ptr<SignalSourceContainer>, std::vector<DataDimension>>
validate_container_for_analysis(std::shared_ptr<SignalSourceContainer> container)
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

std::vector<double> extract_numeric_data_from_container(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container is null or has no data");
    }

    const Kakshya::DataVariant& container_data = container->get_processed_data();

    // Use existing convert_variant_to_double function from DataUtils
    try {
        return convert_variant_to_double(container_data);
    } catch (const std::exception& e) {
        throw std::runtime_error("Cannot extract numeric data from container: " + std::string(e.what()));
    }
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

    // Check for invalid values
    for (size_t i = 0; i < data.size(); ++i) {
        if (!std::isfinite(data[i])) {
            throw std::invalid_argument(operation_name + " data contains NaN or infinite values at index " + std::to_string(i));
        }
    }
}

}
