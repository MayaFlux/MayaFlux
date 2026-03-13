#pragma once

#include <Eigen/Dense>
#include <vector>

namespace MayaFlux::Kinesis {

/**
 * @brief Create N-dimensional rotation matrix
 * @param angle Rotation angle in radians
 * @param axis Rotation axis index (0=X, 1=Y, 2=Z). Only used when dimensions >= 3
 * @param dimensions Dimensionality of the rotation (2 or 3 supported directly,
 *                   higher dimensions return identity)
 * @return Rotation matrix as Eigen::MatrixXd
 */
[[nodiscard]] Eigen::MatrixXd create_rotation_matrix(
    double angle,
    uint32_t axis = 2,
    uint32_t dimensions = 2);

/**
 * @brief Create diagonal scaling matrix from per-axis factors
 * @param scale_factors Scale value per dimension. Empty input returns 1x1 identity.
 * @return Diagonal scaling matrix (NxN where N = scale_factors.size())
 */
[[nodiscard]] Eigen::MatrixXd create_scaling_matrix(
    const std::vector<double>& scale_factors);

/**
 * @brief Create uniform scaling matrix
 * @param scale Uniform scale factor applied to all axes
 * @param dimensions Number of dimensions
 * @return Diagonal scaling matrix (NxN)
 */
[[nodiscard]] Eigen::MatrixXd create_uniform_scaling_matrix(
    double scale,
    uint32_t dimensions);

/**
 * @brief Create translation vector (not a matrix; affine embedding is caller's concern)
 * @param offsets Per-axis translation values
 * @return Column vector of offsets
 */
[[nodiscard]] Eigen::VectorXd create_translation_vector(
    const std::vector<double>& offsets);

/**
 * @brief Compose rotation then scaling: S * R
 * @param angle Rotation angle in radians
 * @param scale_factors Per-axis scale factors (must match dimensions)
 * @param axis Rotation axis index (for 3D)
 * @param dimensions Dimensionality
 * @return Combined transformation matrix
 */
[[nodiscard]] Eigen::MatrixXd create_rotation_scaling_matrix(
    double angle,
    const std::vector<double>& scale_factors,
    uint32_t axis = 2,
    uint32_t dimensions = 2);

} // namespace MayaFlux::Kinesis
