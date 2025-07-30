#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Extract processing state information from container.
 * @param container Container to analyze.
 * @return Map containing processing state metadata.
 */
std::unordered_map<std::string, std::any> extract_processing_state_info(const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Extract processor information from container.
 * @param container Container to analyze.
 * @return Map containing processor metadata.
 */
std::unordered_map<std::string, std::any> extract_processor_info(const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Perform a state transition for a ProcessingState, with optional callback.
 * @param current_state Current state (modified in place).
 * @param new_state State to transition to.
 * @param on_transition Optional callback to invoke on transition.
 * @return True if transition was valid and performed.
 */
bool transition_state(ProcessingState& current_state, ProcessingState new_state, std::function<void()> on_transition = nullptr);

/**
 * @brief Determine optimal memory access pattern for region.
 * @param region Region to analyze.
 * @param container Container providing layout information.
 * @return Recommended access pattern information.
 */
std::unordered_map<std::string, std::any> analyze_access_pattern(const Region& region,
    const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Extract data in interleaved format.
 * @param container The container to extract from.
 * @param pattern Interleaving pattern (e.g., which dimensions to interleave).
 * @return DataVariant containing interleaved data.
 */
DataVariant extract_interleaved_data(const std::shared_ptr<SignalSourceContainer>& container,
    const std::vector<size_t>& pattern = {});

/**
 * @brief Interleave multiple channels of data into a single vector.
 * @tparam T Data type.
 * @param channels Vector of channel vectors.
 * @return Interleaved data vector.
 */
template <typename T>
std::vector<T> interleave_channels(const std::vector<std::vector<T>>& channels)
{
    if (channels.empty()) {
        return {};
    }
    size_t num_channels = channels.size();
    size_t samples_per_channel = channels[0].size();
    std::vector<T> result(num_channels * samples_per_channel);
    for (size_t i = 0; i < samples_per_channel; ++i) {
        for (size_t ch = 0; ch < num_channels; ++ch) {
            result[i * num_channels + ch] = channels[ch][i];
        }
    }
    return result;
}

/**
 * @brief Deinterleave a single vector into multiple channels.
 * @tparam T Data type.
 * @param interleaved Interleaved data span.
 * @param num_channels Number of channels.
 * @return Vector of channel vectors.
 */
template <typename T>
std::vector<std::vector<T>> deinterleave_channels(std::span<const T> interleaved, size_t num_channels)
{
    if (interleaved.empty() || num_channels == 0) {
        return {};
    }
    size_t samples_per_channel = interleaved.size() / num_channels;
    std::vector<std::vector<T>> result(num_channels);
    for (size_t ch = 0; ch < num_channels; ++ch) {
        result[ch].resize(samples_per_channel);
        for (size_t i = 0; i < samples_per_channel; ++i) {
            result[ch][i] = interleaved[i * num_channels + ch];
        }
    }
    return result;
}

/**
 * @brief Extract data from a specific channel.
 * @param container The container to extract from.
 * @param channel_index Index of the channel to extract.
 * @return DataVariant containing channel data.
 */
DataVariant extract_channel_data(const std::shared_ptr<SignalSourceContainer>& container,
    u_int32_t channel_index);

/**
 * @brief Validates container for analysis operations with comprehensive checks
 * @param container Container to validate
 * @return Pair of validated container and its dimensions
 * @throws std::invalid_argument if container is null or has no data
 * @throws std::runtime_error if container has no dimensions
 */
std::pair<std::shared_ptr<SignalSourceContainer>, std::vector<DataDimension>>
validate_container_for_analysis(const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Extracts numeric data from container with fallback handling
 * @param container Container to extract from
 * @return Vector of double values from container data
 * @throws std::runtime_error if no numeric data can be extracted
 */
std::vector<double> extract_numeric_data_from_container(const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Validates numeric data for analysis operations
 * @param data Data to validate
 * @param operation_name Name of operation for error messages
 * @param min_size Minimum required data size (default 1)
 * @throws std::invalid_argument if data is invalid for analysis
 */
void validate_numeric_data_for_analysis(const std::vector<double>& data,
    const std::string& operation_name,
    size_t min_size = 1);

}
