#pragma once

#include <Eigen/Dense>

namespace MayaFlux::Kinesis {

struct BasisMatrices {
    static inline const Eigen::Matrix4d CATMULL_ROM_BASE {
        { -0.5, 1.5, -1.5, 0.5 },
        { 1.0, -2.5, 2.0, -0.5 },
        { -0.5, 0.0, 0.5, 0.0 },
        { 0.0, 1.0, 0.0, 0.0 }
    };

    static inline const Eigen::Matrix4d CUBIC_BEZIER {
        { -1.0, 3.0, -3.0, 1.0 },
        { 3.0, -6.0, 3.0, 0.0 },
        { -3.0, 3.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0, 0.0 }
    };

    static inline const Eigen::Matrix3d QUADRATIC_BEZIER {
        { 1.0, -2.0, 1.0 },
        { -2.0, 2.0, 0.0 },
        { 1.0, 0.0, 0.0 }
    };

    static inline const Eigen::Matrix4d BSPLINE_CUBIC {
        { -1.0 / 6.0, 3.0 / 6.0, -3.0 / 6.0, 1.0 / 6.0 },
        { 3.0 / 6.0, -6.0 / 6.0, 3.0 / 6.0, 0.0 },
        { -3.0 / 6.0, 0.0, 3.0 / 6.0, 0.0 },
        { 1.0 / 6.0, 4.0 / 6.0, 1.0 / 6.0, 0.0 }
    };

    static Eigen::Matrix4d catmull_rom_with_tension(double tension)
    {
        Eigen::Matrix4d m;
        m << -tension, 2.0 - tension, tension - 2.0, tension,
            2.0 * tension, tension - 3.0, 3.0 - 2.0 * tension, -tension,
            -tension, 0.0, tension, 0.0,
            0.0, 1.0, 0.0, 0.0;
        return m;
    }
};

} // namespace MayaFlux::Kinesis
