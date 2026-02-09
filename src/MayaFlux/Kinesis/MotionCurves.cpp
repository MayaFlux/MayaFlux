#include "MotionCurves.hpp"

#include "MayaFlux/Kakshya/NDData/EigenAccess.hpp"
#include "MayaFlux/Kakshya/NDData/EigenInsertion.hpp"

#include "BasisMatrices.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kinesis {

namespace {
    struct SegmentInfo {
        Eigen::Index points_per_segment;
        Eigen::Index overlap;
        bool supports_multi;
    };

    SegmentInfo get_segment_info(InterpolationMode mode)
    {
        switch (mode) {
        case InterpolationMode::LINEAR:
        case InterpolationMode::COSINE:
            return {
                .points_per_segment = 2,
                .overlap = 1,
                .supports_multi = true
            };

        case InterpolationMode::CATMULL_ROM:
        case InterpolationMode::BSPLINE:
            return {
                .points_per_segment = 4,
                .overlap = 3,
                .supports_multi = true
            };

        case InterpolationMode::CUBIC_BEZIER:
            return {
                .points_per_segment = 4,
                .overlap = 1,
                .supports_multi = true
            };

        case InterpolationMode::QUADRATIC_BEZIER:
            return {
                .points_per_segment = 3,
                .overlap = 1,
                .supports_multi = true
            };

        case InterpolationMode::CUBIC_HERMITE:
            return {
                .points_per_segment = 4,
                .overlap = 0,
                .supports_multi = false
            };

        default:
            return {
                .points_per_segment = 0,
                .overlap = 0,
                .supports_multi = false
            };
        }
    }

    struct ExtendedControls {
        const Eigen::MatrixXd* controls;
        Eigen::Index count;
        Eigen::MatrixXd storage;

        ExtendedControls(const Eigen::MatrixXd& original, InterpolationMode mode, Eigen::Index points_per_segment)
            : controls(&original)
            , count(original.cols())
        {
            if ((mode == InterpolationMode::CATMULL_ROM || mode == InterpolationMode::BSPLINE)
                && count > points_per_segment) {
                storage.resize(original.rows(), count + 2);
                storage.col(0) = original.col(0);
                storage.col(count + 1) = original.col(count - 1);
                storage.middleCols(1, count) = original;

                controls = &storage;
                count = count + 2;
            }
        }
    };

    struct SegmentLocation {
        Eigen::Index seg_idx;
        double t_local;

        static SegmentLocation compute(
            Eigen::Index sample_idx,
            Eigen::Index num_samples,
            Eigen::Index num_segments)
        {
            double t_global = static_cast<double>(sample_idx) / static_cast<double>(num_samples - 1);
            double segment_float = t_global * static_cast<double>(num_segments);
            auto seg_idx = static_cast<Eigen::Index>(std::floor(segment_float));
            double t_local = segment_float - static_cast<double>(seg_idx);

            if (sample_idx == num_samples - 1 || seg_idx >= num_segments) {
                seg_idx = num_segments - 1;
                t_local = 1.0;
            }

            return {
                .seg_idx = seg_idx,
                .t_local = t_local
            };
        }
    };

    Eigen::MatrixXd interpolate_single_segment(
        const Eigen::MatrixXd& control_points,
        Eigen::Index num_samples,
        InterpolationMode mode,
        double tension)
    {
        Eigen::MatrixXd result(control_points.rows(), num_samples);
        for (Eigen::Index i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / static_cast<double>(num_samples - 1);
            result.col(i) = interpolate(control_points, t, mode, tension);
        }
        return result;
    }

    Eigen::MatrixXd extract_segment_controls(
        const Eigen::MatrixXd& active_controls,
        Eigen::Index active_num_controls,
        Eigen::Index seg_idx,
        Eigen::Index points_per_segment,
        Eigen::Index overlap,
        double& t_local)
    {
        Eigen::Index start_col = seg_idx * (points_per_segment - overlap);

        if (start_col + points_per_segment > active_num_controls) {
            start_col = active_num_controls - points_per_segment;
            t_local = 1.0;
        }

        return active_controls.middleCols(start_col, points_per_segment);
    }

    Eigen::Index compute_num_segments(
        Eigen::Index num_controls,
        Eigen::Index points_per_segment,
        Eigen::Index overlap)
    {
        return (overlap == 0)
            ? num_controls / points_per_segment
            : (num_controls - overlap) / (points_per_segment - overlap);
    }
}

Eigen::VectorXd catmull_rom_spline(
    const Eigen::MatrixXd& control_points,
    double t,
    double tension)
{
    if (control_points.cols() != 4) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Catmull-Rom interpolation requires 4 control points, but got {}",
            control_points.cols());
    }

    Eigen::Matrix4d basis_matrix = BasisMatrices::catmull_rom_with_tension(tension);
    Eigen::Vector4d t_vector(t * t * t, t * t, t, 1.0);
    Eigen::Vector4d coeffs = basis_matrix * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd cubic_bezier(
    const Eigen::MatrixXd& control_points,
    double t)
{
    if (control_points.cols() != 4) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cubic Bezier interpolation requires 4 control points, but got {}",
            control_points.cols());
    }

    Eigen::Vector4d t_vector(t * t * t, t * t, t, 1.0);
    Eigen::Vector4d coeffs = BasisMatrices::CUBIC_BEZIER * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd quadratic_bezier(
    const Eigen::MatrixXd& control_points,
    double t)
{
    if (control_points.cols() != 3) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Quadratic Bezier interpolation requires 3 control points, but got {}",
            control_points.cols());
    }

    Eigen::Vector3d t_vector(t * t, t, 1.0);
    Eigen::Vector3d coeffs = BasisMatrices::QUADRATIC_BEZIER * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd cubic_hermite(
    const Eigen::MatrixXd& endpoints,
    const Eigen::MatrixXd& tangents,
    double t)
{
    if (endpoints.cols() != 2 || tangents.cols() != 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cubic Hermite interpolation requires 2 endpoints and 2 tangents, but got {} endpoints and {} tangents",
            endpoints.cols(), tangents.cols());
    }

    double t2 = t * t;
    double t3 = t2 * t;

    double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    double h10 = t3 - 2.0 * t2 + t;
    double h01 = -2.0 * t3 + 3.0 * t2;
    double h11 = t3 - t2;

    return h00 * endpoints.col(0) + h10 * tangents.col(0) + h01 * endpoints.col(1) + h11 * tangents.col(1);
}

Eigen::VectorXd bspline_cubic(
    const Eigen::MatrixXd& control_points,
    double t)
{
    if (control_points.cols() != 4) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cubic B-spline interpolation requires 4 control points, but got {}",
            control_points.cols());
    }

    Eigen::Vector4d t_vector(t * t * t, t * t, t, 1.0);
    Eigen::Vector4d coeffs = BasisMatrices::BSPLINE_CUBIC * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd interpolate(
    const Eigen::MatrixXd& control_points,
    double t,
    InterpolationMode mode,
    double tension)
{
    switch (mode) {
    case InterpolationMode::LINEAR:
        if (control_points.cols() < 2) {
            error<std::invalid_argument>(
                Journal::Component::Kinesis,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Linear interpolation requires at least 2 points, but got {}",
                control_points.cols());
        }
        return (1.0 - t) * control_points.col(0) + t * control_points.col(1);

    case InterpolationMode::CATMULL_ROM:
        return catmull_rom_spline(control_points, t, tension);

    case InterpolationMode::CUBIC_HERMITE: {
        Eigen::MatrixXd endpoints = control_points.leftCols(2);
        Eigen::MatrixXd tangents = control_points.rightCols(2);
        return cubic_hermite(endpoints, tangents, t);
    }

    case InterpolationMode::CUBIC_BEZIER:
        return cubic_bezier(control_points, t);

    case InterpolationMode::QUADRATIC_BEZIER:
        return quadratic_bezier(control_points, t);

    case InterpolationMode::BSPLINE:
        return bspline_cubic(control_points, t);

    case InterpolationMode::COSINE: {
        if (control_points.cols() < 2) {
            error<std::invalid_argument>(
                Journal::Component::Kinesis,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Cosine interpolation requires at least 2 points, but got {}",
                control_points.cols());
        }
        double mu2 = (1.0 - std::cos(t * M_PI)) * 0.5;
        return (1.0 - mu2) * control_points.col(0) + mu2 * control_points.col(1);
    }

    default:
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Unsupported interpolation mode: {}",
            static_cast<int>(mode));
    }
}

Eigen::MatrixXd generate_interpolated_points(
    const Eigen::MatrixXd& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension)
{
    if (num_samples < 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "num_samples must be at least 2, but got {}",
            num_samples);
    }

    if (control_points.cols() < 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Need at least 2 control points, but got {}",
            control_points.cols());
    }

    auto [points_per_segment, overlap, supports_multi] = get_segment_info(mode);

    if (!supports_multi) {
        if (control_points.cols() != points_per_segment) {
            error<std::invalid_argument>(
                Journal::Component::Kinesis,
                Journal::Context::Runtime,
                std::source_location::current(),
                "{} interpolation requires exactly {} control points, but got {}",
                static_cast<int>(mode), points_per_segment, control_points.cols());
        }
        return interpolate_single_segment(control_points, num_samples, mode, tension);
    }

    if (control_points.cols() == points_per_segment) {
        return interpolate_single_segment(control_points, num_samples, mode, tension);
    }

    ExtendedControls extended(control_points, mode, points_per_segment);
    Eigen::Index num_segments = compute_num_segments(extended.count, points_per_segment, overlap);

    if (num_segments < 1) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Need sufficient control points for multi-segment {} interpolation, but got {}",
            static_cast<int>(mode), control_points.cols());
    }

    Eigen::MatrixXd result(control_points.rows(), num_samples);

    for (Eigen::Index i = 0; i < num_samples; ++i) {
        auto [seg_idx, t_local] = SegmentLocation::compute(i, num_samples, num_segments);

        Eigen::MatrixXd segment_controls = extract_segment_controls(
            *extended.controls,
            extended.count,
            seg_idx,
            points_per_segment,
            overlap,
            t_local);

        result.col(i) = interpolate(segment_controls, t_local, mode, tension);
    }

    return result;
}

double compute_arc_length(const Eigen::MatrixXd& points)
{
    if (points.cols() < 2) {
        return 0.0;
    }

    double length = 0.0;
    for (Eigen::Index i = 1; i < points.cols(); ++i) {
        length += (points.col(i) - points.col(i - 1)).norm();
    }

    return length;
}

Eigen::VectorXd compute_arc_length_table(const Eigen::MatrixXd& points)
{
    Eigen::VectorXd arc_lengths(points.cols());
    arc_lengths(0) = 0.0;

    for (Eigen::Index i = 1; i < points.cols(); ++i) {
        arc_lengths(i) = arc_lengths(i - 1) + (points.col(i) - points.col(i - 1)).norm();
    }

    return arc_lengths;
}

Eigen::MatrixXd reparameterize_by_arc_length(
    const Eigen::MatrixXd& points,
    Eigen::Index num_samples)
{
    Eigen::VectorXd arc_lengths = compute_arc_length_table(points);
    double total_length = arc_lengths(arc_lengths.size() - 1);

    if (total_length == 0.0) {
        return points;
    }

    Eigen::MatrixXd result(points.rows(), num_samples);

    for (Eigen::Index i = 0; i < num_samples; ++i) {
        double target = (static_cast<double>(i) / static_cast<double>(num_samples - 1)) * total_length;

        Eigen::Index idx = 0;
        for (Eigen::Index j = 1; j < arc_lengths.size(); ++j) {
            if (arc_lengths(j) >= target) {
                idx = j - 1;
                break;
            }
        }

        if (idx + 1 < points.cols()) {
            double segment_start = arc_lengths(idx);
            double segment_end = arc_lengths(idx + 1);
            double t = (target - segment_start) / (segment_end - segment_start);

            result.col(i) = (1.0 - t) * points.col(idx) + t * points.col(idx + 1);
        } else {
            result.col(i) = points.col(points.cols() - 1);
        }
    }

    return result;
}

Kakshya::DataVariant interpolate_nddata(
    const Kakshya::DataVariant& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension)
{
    Eigen::MatrixXd control_matrix = Kakshya::to_eigen_matrix(control_points);

    Eigen::MatrixXd interpolated = generate_interpolated_points(
        control_matrix, num_samples, mode, tension);

    Kakshya::EigenAccess input_access(control_points);

    if (input_access.is_complex()) {
        return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::COMPLEX);
    }

    if (input_access.is_structured()) {
        switch (input_access.component_count()) {
        case 2:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::VEC2);
        case 3:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::VEC3);
        case 4:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::VEC4);
        default:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::SCALAR);
        }
    } else {
        return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::SCALAR);
    }
}

} // namespace MayaFlux::Kinesis
