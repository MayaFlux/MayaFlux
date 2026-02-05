#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include <Eigen/Dense>

namespace MayaFlux::Kinesis {

/**
 * @enum InterpolationMode
 * @brief Mathematical interpolation methods
 */
enum class InterpolationMode : uint8_t {
    LINEAR,
    CATMULL_ROM,
    CUBIC_HERMITE,
    CUBIC_BEZIER,
    QUADRATIC_BEZIER,
    BSPLINE,
    COSINE,
    CUSTOM
};

/**
 * @brief Catmull-Rom spline interpolation using Eigen matrices
 * @param control_points 4xN matrix where columns are control points (p0, p1, p2, p3)
 * @param t Parameter in [0,1]
 * @param tension Tension parameter (default 0.5)
 * @return Interpolated point as Nx1 vector
 */
Eigen::VectorXd catmull_rom_spline(
    const Eigen::MatrixXd& control_points,
    double t,
    double tension = 0.5);

/**
 * @brief Cubic Bezier interpolation using Eigen matrices
 * @param control_points 4xN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
Eigen::VectorXd cubic_bezier(
    const Eigen::MatrixXd& control_points,
    double t);

/**
 * @brief Quadratic Bezier interpolation using Eigen matrices
 * @param control_points 3xN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
Eigen::VectorXd quadratic_bezier(
    const Eigen::MatrixXd& control_points,
    double t);

/**
 * @brief Cubic Hermite interpolation using Eigen matrices
 * @param endpoints 2xN matrix (start, end)
 * @param tangents 2xN matrix (tangent_start, tangent_end)
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
Eigen::VectorXd cubic_hermite(
    const Eigen::MatrixXd& endpoints,
    const Eigen::MatrixXd& tangents,
    double t);

/**
 * @brief Uniform B-spline interpolation using Eigen matrices
 * @param control_points 4xN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
Eigen::VectorXd bspline_cubic(
    const Eigen::MatrixXd& control_points,
    double t);

/**
 * @brief Generic interpolation dispatcher
 * @param control_points MxN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @param mode Interpolation mode
 * @param tension Tension parameter (for applicable modes)
 * @return Interpolated point as Nx1 vector
 */
Eigen::VectorXd interpolate(
    const Eigen::MatrixXd& control_points,
    double t,
    InterpolationMode mode,
    double tension = 0.5);

/**
 * @brief Generate interpolated points from control points
 * @param control_points MxN matrix where columns are control points
 * @param num_samples Number of interpolated points to generate
 * @param mode Interpolation mode
 * @param tension Tension parameter
 * @return Matrix where columns are interpolated points
 */
Eigen::MatrixXd generate_interpolated_points(
    const Eigen::MatrixXd& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension = 0.5);

/**
 * @brief Compute arc length of curve using trapezoidal rule
 * @param points Columns are sequential points along curve
 * @return Estimated arc length
 */
double compute_arc_length(const Eigen::MatrixXd& points);

/**
 * @brief Compute arc length parameterization table
 * @param points Columns are sequential points along curve
 * @return Vector of cumulative arc lengths
 */
Eigen::VectorXd compute_arc_length_table(const Eigen::MatrixXd& points);

/**
 * @brief Reparameterize curve by arc length
 * @param points Original points (columns)
 * @param num_samples Number of output samples
 * @return Arc-length parameterized points
 */
Eigen::MatrixXd reparameterize_by_arc_length(
    const Eigen::MatrixXd& points,
    Eigen::Index num_samples);

/**
 * @brief Process DataVariant through interpolation
 * @param control_points Input data as control points
 * @param num_samples Number of output samples
 * @param mode Interpolation mode
 * @param tension Tension parameter
 * @return Interpolated data
 */
Kakshya::DataVariant interpolate_nddata(
    const Kakshya::DataVariant& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension = 0.5);

} // namespace MayaFlux::Kinesis
