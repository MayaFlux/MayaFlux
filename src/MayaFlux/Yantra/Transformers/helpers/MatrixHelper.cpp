#include "MatrixHelper.hpp"

namespace MayaFlux::Yantra {

Eigen::MatrixXd create_rotation_matrix(double angle, u_int32_t axis, u_int32_t dimensions)
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

    Eigen::MatrixXd scaling = Eigen::MatrixXd::Zero(scale_factors.size(), scale_factors.size());

    auto indices = std::views::iota(size_t { 0 }, scale_factors.size());
    std::ranges::for_each(indices, [&](size_t i) {
        scaling(i, i) = scale_factors[i];
    });

    return scaling;
}

} // namespace MayaFlux::Yantra
