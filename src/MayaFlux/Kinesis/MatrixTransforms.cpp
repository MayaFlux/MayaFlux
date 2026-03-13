#include "MatrixTransforms.hpp"

namespace MayaFlux::Kinesis {

Eigen::MatrixXd create_rotation_matrix(double angle, uint32_t axis, uint32_t dimensions)
{
    if (dimensions == 2) {
        Eigen::Matrix2d rotation;
        rotation << std::cos(angle), -std::sin(angle),
            std::sin(angle), std::cos(angle);
        return rotation;
    }

    if (dimensions == 3) {
        Eigen::Matrix3d rotation = Eigen::Matrix3d::Identity();

        switch (axis) {
        case 0:
            rotation << 1, 0, 0,
                0, std::cos(angle), -std::sin(angle),
                0, std::sin(angle), std::cos(angle);
            break;
        case 1:
            rotation << std::cos(angle), 0, std::sin(angle),
                0, 1, 0,
                -std::sin(angle), 0, std::cos(angle);
            break;
        case 2:
        default:
            rotation << std::cos(angle), -std::sin(angle), 0,
                std::sin(angle), std::cos(angle), 0,
                0, 0, 1;
            break;
        }
        return rotation;
    }

    return Eigen::MatrixXd::Identity(dimensions, dimensions);
}

Eigen::MatrixXd create_scaling_matrix(const std::vector<double>& scale_factors)
{
    if (scale_factors.empty()) {
        return Eigen::MatrixXd::Identity(1, 1);
    }

    Eigen::MatrixXd scaling = Eigen::MatrixXd::Zero(
        scale_factors.size(), scale_factors.size());

    for (size_t i = 0; i < scale_factors.size(); ++i) {
        scaling(i, i) = scale_factors[i];
    }

    return scaling;
}

Eigen::MatrixXd create_uniform_scaling_matrix(double scale, uint32_t dimensions)
{
    return Eigen::MatrixXd::Identity(dimensions, dimensions) * scale;
}

Eigen::VectorXd create_translation_vector(const std::vector<double>& offsets)
{
    Eigen::VectorXd vec(offsets.size());
    for (size_t i = 0; i < offsets.size(); ++i) {
        vec(static_cast<Eigen::Index>(i)) = offsets[i];
    }
    return vec;
}

Eigen::MatrixXd create_rotation_scaling_matrix(
    double angle,
    const std::vector<double>& scale_factors,
    uint32_t axis,
    uint32_t dimensions)
{
    return create_scaling_matrix(scale_factors) * create_rotation_matrix(angle, axis, dimensions);
}

} // namespace MayaFlux::Kinesis
