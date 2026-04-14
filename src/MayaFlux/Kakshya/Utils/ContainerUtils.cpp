#include "ContainerUtils.hpp"
#include "DataUtils.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "RegionUtils.hpp"

#include "MayaFlux/Kakshya/StreamContainer.hpp"

namespace MayaFlux::Kakshya {

std::unordered_map<std::string, std::any> extract_processing_state_info(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Container is null");
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
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Container is null");
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
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Container is null");
    }

    const auto dimensions = container->get_dimensions();
    const auto memory_layout = container->get_memory_layout();

    analysis["is_contiguous"] = is_region_access_contiguous(region, container);
    analysis["memory_layout"] = static_cast<int>(memory_layout);

    uint64_t region_size = 1;
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
    uint32_t channel_index)
{
    if (!container) {
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Container is null");
    }

    const auto structure = container->get_structure();
    auto channel_count = structure.get_channel_count();

    if (channel_index >= channel_count) {
        error<std::out_of_range>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Channel index out of range: {} (available channels: {})", channel_index, channel_count);
    }

    auto data = container->get_data();

    if (structure.organization == OrganizationStrategy::PLANAR) {
        if (channel_index >= data.size()) {
            error<std::out_of_range>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Channel index out of range for planar data: {} (available channels: {})", channel_index, data.size());
        }
        return data[channel_index];
    }

    std::vector<double> temp_storage;
    auto interleaved_span = extract_from_variant<double>(data[0], temp_storage);

    if (interleaved_span.empty()) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Failed to extract interleaved data");
    }

    if (interleaved_span.size() % channel_count != 0) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Interleaved data size ({}) is not a multiple of channel count ({})", interleaved_span.size(), channel_count);
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

void extract_processed_data(
    const std::vector<DataVariant>& pd,
    OrganizationStrategy organization,
    uint64_t num_channels,
    uint32_t ch,
    std::span<double> output)
{
    if (pd.empty() || output.empty()) {
        std::ranges::fill(output, 0.0);
        return;
    }

    if (organization == OrganizationStrategy::INTERLEAVED) {
        thread_local std::vector<double> tmp;
        auto data_span = extract_from_variant<double>(pd[0], tmp);

        const auto samples_to_copy = std::min<size_t>(
            output.size(),
            data_span.size() / std::max<uint64_t>(num_channels, 1));

        for (auto s : std::views::iota(0UZ, samples_to_copy)) {
            const auto idx = s * num_channels + ch;
            output[s] = idx < data_span.size() ? data_span[idx] : 0.0;
        }

        if (samples_to_copy < output.size())
            std::ranges::fill(output.subspan(samples_to_copy), 0.0);
    } else {
        if (ch >= pd.size()) {
            std::ranges::fill(output, 0.0);
            return;
        }

        thread_local std::vector<double> tmp;
        auto ch_span = extract_from_variant<double>(pd[ch], tmp);

        const auto samples_to_copy = std::min(output.size(), ch_span.size());
        std::ranges::copy_n(ch_span.begin(), samples_to_copy, output.begin());

        if (samples_to_copy < output.size())
            std::ranges::fill(output.subspan(samples_to_copy), 0.0);
    }
}

std::vector<double> extract_region_channel(
    const Region& region,
    const std::shared_ptr<SignalSourceContainer>& container,
    uint32_t channel)
{
    if (!container)
        return {};

    auto variants = container->get_region_data(region);

    if (channel >= variants.size())
        return {};

    std::vector<double> storage;
    Kakshya::extract_from_variant<double>(variants[channel], storage);
    return storage;
}

std::pair<std::shared_ptr<SignalSourceContainer>, std::vector<DataDimension>>
validate_container_for_analysis(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container || !container->has_data()) {
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Container is null or has no data");
    }

    auto dimensions = container->get_dimensions();
    if (dimensions.empty()) {
        error<std::runtime_error>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Container has no dimensions");
    }

    return std::make_pair(container, std::move(dimensions));
}

std::vector<std::span<double>> extract_numeric_data(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container || !container->has_data()) {
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Container is null or has no data");
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
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "Cannot perform {} on empty data", operation_name);
    }

    if (data.size() < min_size) {
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "{} requires at least {} data points, got {}", operation_name, min_size, data.size());
    }

    if (auto invalid_it = std::ranges::find_if_not(data, [](double val) {
            return std::isfinite(val);
        });
        invalid_it != data.end()) {

        const auto index = std::distance(data.begin(), invalid_it);
        error<std::invalid_argument>(Journal::Component::Kakshya, Journal::Context::Runtime, std::source_location::current(), "{} data contains NaN or infinite values at index {}", operation_name, index);
    }
}

}
