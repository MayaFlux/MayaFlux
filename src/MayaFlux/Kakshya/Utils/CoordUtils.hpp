#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Convert N-dimensional coordinates to a linear index for interleaved data.
 * @param coords N-dimensional coordinates.
 * @param dimensions Dimension descriptors (size/stride/role).
 * @return Linear index into the underlying data storage.
 * @note Only works with interleaved organization strategy. For planar data,
 *       use coordinates_to_planar_indices() instead.
 */
u_int64_t coordinates_to_linear(const std::vector<u_int64_t>& coords, const std::vector<DataDimension>& dimensions);

/**
 * @brief Convert a linear index to N-dimensional coordinates for interleaved data.
 * @param index Linear index into the underlying data storage.
 * @param dimensions Dimension descriptors (size/stride/role).
 * @return N-dimensional coordinates.
 * @note Only works with interleaved organization strategy. For planar data,
 *       coordinates map directly to {channel_vector_index, frame_index}.
 */
std::vector<u_int64_t> linear_to_coordinates(u_int64_t index, const std::vector<DataDimension>& dimensions);

/**
 * @brief Calculate memory strides for each dimension (row-major order).
 * @param dimensions Dimension descriptors.
 * @return Vector of strides for each dimension.
 */
std::vector<u_int64_t> calculate_strides(const std::vector<DataDimension>& dimensions);

/**
 * @brief Validate region bounds against container dimensions.
 * @param region Region to validate.
 * @param dimensions Container dimensions.
 * @return True if region is valid, false otherwise.
 */
bool validate_region_bounds(const Region& region, const std::vector<DataDimension>& dimensions);

/**
 * @brief Validate slice coordinates against container bounds.
 * @param slice_start Starting coordinates.
 * @param slice_end Ending coordinates.
 * @param dimensions Container dimensions.
 * @return True if slice is valid, false otherwise.
 */
bool validate_slice_bounds(const std::vector<u_int64_t>& slice_start,
    const std::vector<u_int64_t>& slice_end,
    const std::vector<DataDimension>& dimensions);

/**
 * @brief Clamp coordinates to valid container bounds.
 * @param coords Coordinates to clamp (modified in place).
 * @param dimensions Container dimensions for bounds.
 */
void clamp_coordinates_to_bounds(std::vector<u_int64_t>& coords,
    const std::vector<DataDimension>& dimensions);

/**
 * @brief Transform coordinates using scaling, translation, rotation.
 * @param coords Input coordinates.
 * @param scale_factors Scaling factors per dimension.
 * @param offset_values Translation offset per dimension.
 * @param rotation_params Optional rotation parameters.
 * @return Transformed coordinates.
 */
std::vector<u_int64_t> transform_coordinates(const std::vector<u_int64_t>& coords,
    const std::vector<double>& scale_factors = {},
    const std::vector<int64_t>& offset_values = {},
    const std::unordered_map<std::string, std::any>& rotation_params = {});

/**
 * @brief Wrap a position within a loop range if looping is enabled.
 * @param position Current position.
 * @param loop_start Loop start position.
 * @param loop_end Loop end position.
 * @param looping_enabled Whether looping is enabled.
 * @return Wrapped position.
 */
u_int64_t wrap_position_with_loop(u_int64_t position, u_int64_t loop_start, u_int64_t loop_end, bool looping_enabled);

/**
 * @brief Wrap a position within a loop region and dimension if looping is enabled.
 * @param position Current position.
 * @param loop_region Region defining the loop.
 * @param dim Dimension index.
 * @param looping_enabled Whether looping is enabled.
 * @return Wrapped position.
 */
u_int64_t wrap_position_with_loop(u_int64_t position, const Region& loop_region, size_t dim, bool looping_enabled);

/**
 * @brief Advance a position by a given amount, with optional looping.
 * @param current_pos Current position.
 * @param advance_amount Amount to advance.
 * @param total_size Total size of the dimension.
 * @param loop_start Loop start position.
 * @param loop_end Loop end position.
 * @param looping Whether looping is enabled.
 * @return New position.
 */
u_int64_t advance_position(u_int64_t current_pos, u_int64_t advance_amount, u_int64_t total_size, u_int64_t loop_start, u_int64_t loop_end, bool looping);

/**
 * @brief Convert time (seconds) to position (samples/frames) given a sample rate.
 * @param time Time in seconds.
 * @param sample_rate Sample rate (Hz).
 * @return Position as integer index.
 */
u_int64_t time_to_position(double time, double sample_rate);

/**
 * @brief Convert position (samples/frames) to time (seconds) given a sample rate.
 * @param position Position as integer index.
 * @param sample_rate Sample rate (Hz).
 * @return Time in seconds.
 */
double position_to_time(u_int64_t position, double sample_rate);

/**
 * @brief Calculate the frame size for a specific primary dimension.
 * @param dimensions Dimension descriptors.
 * @param primary_dim Index of the primary dimension.
 * @return Frame size (product of all but the primary dimension).
 */
u_int64_t calculate_frame_size_for_dimension(const std::vector<DataDimension>& dimensions, size_t primary_dim = 0);

/**
 * @brief Extract dimension roles as integers.
 * @param dimensions Container dimensions.
 * @return Vector of role values as integers.
 */
std::vector<int> extract_dimension_roles(const std::vector<DataDimension>& dimensions);

/**
 * @brief Extract dimension sizes.
 * @param dimensions Container dimensions.
 * @return Vector of dimension sizes.
 */
std::vector<u_int64_t> extract_dimension_sizes(const std::vector<DataDimension>& dimensions);

/**
 * @brief Create structured dimension information.
 * @param dimensions Container dimensions.
 * @return Vector of maps containing dimension metadata.
 */
std::vector<std::unordered_map<std::string, std::any>> create_dimension_info(const std::vector<DataDimension>& dimensions);

/**
 * @brief Create coordinate mapping information for container.
 * @param container The container to analyze.
 * @return Map containing coordinate mapping metadata.
 */
std::unordered_map<std::string, std::any> create_coordinate_mapping(const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Convert coordinates to planar indices (channel vector + frame index).
 * @param coords N-dimensional coordinates.
 * @param dimensions Dimension descriptors.
 * @return Pair of {channel_index, frame_index} for planar access.
 * @note Only works with planar organization strategy.
 */
std::pair<size_t, u_int64_t> coordinates_to_planar_indices(
    const std::vector<u_int64_t>& coords,
    const std::vector<DataDimension>& dimensions);
}
