#include "ContainerUtils.hpp"
#include "DataUtils.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "RegionUtils.hpp"

#include "MayaFlux/Kakshya/StreamContainer.hpp"

#include "ranges"

namespace MayaFlux::Kakshya {

std::unordered_map<std::string, std::any> extract_processing_state_info(const std::shared_ptr<SignalSourceContainer>& container)
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

DataVariant extract_interleaved_data(const std::shared_ptr<SignalSourceContainer>& container,
    const std::vector<size_t>& pattern)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto dimensions = container->get_dimensions();
    std::vector<size_t> interleave_pattern = pattern;

    if (interleave_pattern.empty() && dimensions.size() >= 2) {
        auto channel_it = std::ranges::find_if(dimensions,
            [](const DataDimension& dim) {
                return dim.role == DataDimension::Role::CHANNEL;
            });

        if (channel_it != dimensions.end()) {
#ifdef MAYAFLUX_PLATFORM_MACOS
            interleave_pattern.clear();
            interleave_pattern.reserve(channel_it->size);
            // std::ranges::copy(channel_indices, std::back_inserter(interleave_pattern));
            for (size_t i = 0; i < channel_it->size; ++i) {
                interleave_pattern.push_back(i);
            }
#endif // MAYAFLUX_PLATFORM_MACOS
            auto channel_indices = std::ranges::views::iota(size_t { 0 }, channel_it->size);
            interleave_pattern.assign(channel_indices.begin(), channel_indices.end());
        }
    }

    if (interleave_pattern.empty()) {
        return container->get_processed_data(); // No interleaving needed
    }

    std::vector<DataVariant> channel_data;
    channel_data.reserve(interleave_pattern.size());

    std::ranges::transform(interleave_pattern, std::back_inserter(channel_data),
        [&container](size_t ch_idx) {
            return extract_channel_data(container, static_cast<u_int32_t>(ch_idx));
        });

    return std::visit([&channel_data](const auto& first_channel) -> DataVariant {
        using T = typename std::decay_t<decltype(first_channel)>::value_type;
        std::vector<T> interleaved;

        size_t samples_per_channel = first_channel.size();
        interleaved.reserve(samples_per_channel * channel_data.size());

        for (size_t sample = 0; sample < samples_per_channel; ++sample) {
            for (auto& ch : channel_data) {
                std::visit([&interleaved, sample](const auto& channel_vec) {
                    using ChannelType = typename std::decay_t<decltype(channel_vec)>::value_type;
                    if constexpr (std::is_same_v<ChannelType, T>) {
                        if (sample < channel_vec.size()) {
                            interleaved.push_back(channel_vec[sample]);
                        }
                    }
                },
                    ch);
            }
        }

        return DataVariant { interleaved };
    },
        channel_data.front());
}

DataVariant extract_channel_data(const std::shared_ptr<SignalSourceContainer>& container,
    u_int32_t channel_index)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto dimensions = container->get_dimensions();
    if (dimensions.size() < 2) {
        throw std::invalid_argument("Container must have at least 2 dimensions for channel extraction");
    }

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
        channel_dim_index = 1;
    }

    if (channel_index >= dimensions[channel_dim_index].size) {
        throw std::out_of_range("Channel index out of range");
    }

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

std::vector<double> extract_numeric_data_from_container(const std::shared_ptr<SignalSourceContainer>& container)
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

    if (auto invalid_it = std::ranges::find_if_not(data, [](double val) {
            return std::isfinite(val);
        });
        invalid_it != data.end()) {

        const auto index = std::distance(data.begin(), invalid_it);
        throw std::invalid_argument(operation_name + " data contains NaN or infinite values at index " + std::to_string(index));
    }
}

}
