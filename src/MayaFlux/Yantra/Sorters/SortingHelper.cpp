#include "SortingHelper.hpp"

#include "Eigen/Eigenvalues"

namespace MayaFlux::Yantra {

void sort_data_variant_inplace(Kakshya::DataVariant& data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    std::visit([direction, algorithm](auto& vec) {
        using VecType = std::decay_t<decltype(vec)>;

        if constexpr (SortableContainerType<VecType>) {
            sort_container(vec, direction, algorithm);
        }
    },
        data);
}

Kakshya::DataVariant sort_data_variant_extract(const Kakshya::DataVariant& data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    return std::visit([direction, algorithm](const auto& vec) -> Kakshya::DataVariant {
        using VecType = std::decay_t<decltype(vec)>;

        if constexpr (SortableContainerType<VecType>) {
            auto sorted_vec = vec;
            sort_container(sorted_vec, direction, algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else {
            return Kakshya::DataVariant { vec };
        }
    },
        data);
}

bool is_data_variant_sortable(const Kakshya::DataVariant& data)
{
    return std::visit([](const auto& vec) -> bool {
        using VecType = std::decay_t<decltype(vec)>;
        return SortableContainerType<VecType>;
    },
        data);
}

std::vector<size_t> get_data_variant_sort_indices(const Kakshya::DataVariant& data,
    SortingDirection direction)
{
    return std::visit([direction](const auto& vec) -> std::vector<size_t> {
        using VecType = std::decay_t<decltype(vec)>;
        using ValueType = typename VecType::value_type;

        if constexpr (SortableContainerType<VecType> && StandardSortable<ValueType>) {
            auto comp = create_standard_comparator<ValueType>(direction);
            return generate_sort_indices(vec, comp);
        } else if constexpr (SortableContainerType<VecType> && ComplexSortable<ValueType>) {
            auto comp = create_complex_magnitude_comparator<ValueType>(direction);
            return generate_sort_indices(vec, comp);
        } else {
            std::vector<size_t> indices(vec.size());
            std::iota(indices.begin(), indices.end(), 0);
            return indices;
        }
    },
        data);
}

void sort_region_group_by_dimension(Kakshya::RegionGroup& group,
    size_t dimension_index,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    auto comp = create_coordinate_comparator<Kakshya::Region>(direction, dimension_index);
    execute_sorting_algorithm(group.regions.begin(), group.regions.end(), comp, algorithm);
}

void sort_region_group_by_duration(Kakshya::RegionGroup& group,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    auto comp = [direction](const Kakshya::Region& a, const Kakshya::Region& b) -> bool {
        if (!a.start_coordinates.empty() && !a.end_coordinates.empty() && !b.start_coordinates.empty() && !b.end_coordinates.empty()) {
            auto duration_a = a.end_coordinates[0] - a.start_coordinates[0];
            auto duration_b = b.end_coordinates[0] - b.start_coordinates[0];
            return direction == SortingDirection::ASCENDING ? duration_a < duration_b : duration_a > duration_b;
        }
        return false;
    };
    execute_sorting_algorithm(group.regions.begin(), group.regions.end(), comp, algorithm);
}

void sort_segments_by_duration(std::vector<Kakshya::RegionSegment>& segments,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    auto comp = [direction](const Kakshya::RegionSegment& a, const Kakshya::RegionSegment& b) -> bool {
        const auto& region_a = a.source_region;
        const auto& region_b = b.source_region;

        if (!region_a.start_coordinates.empty() && !region_a.end_coordinates.empty() && !region_b.start_coordinates.empty() && !region_b.end_coordinates.empty()) {
            auto duration_a = region_a.end_coordinates[0] - region_a.start_coordinates[0];
            auto duration_b = region_b.end_coordinates[0] - region_b.start_coordinates[0];
            return direction == SortingDirection::ASCENDING ? duration_a < duration_b : duration_a > duration_b;
        }
        return false;
    };
    execute_sorting_algorithm(segments.begin(), segments.end(), comp, algorithm);
}

void sort_eigen_vector(Eigen::VectorXd& vector,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    std::span<double> data_span(vector.data(), vector.size());
    auto comp = create_standard_comparator<double>(direction);
    execute_sorting_algorithm(data_span.begin(), data_span.end(), comp, algorithm);
}

void sort_eigen_matrix_by_rows(Eigen::MatrixXd& matrix,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    for (int row = 0; row < matrix.rows(); ++row) {
        auto matrix_row = matrix.row(row);
        std::vector<double> row_data(matrix_row.data(), matrix_row.data() + matrix_row.size());
        auto comp = create_standard_comparator<double>(direction);

        execute_sorting_algorithm(row_data.begin(), row_data.end(), comp, algorithm);
        matrix.row(row) = Eigen::Map<Eigen::VectorXd>(row_data.data(), row_data.size());
    }
}

void sort_eigen_matrix_by_columns(Eigen::MatrixXd& matrix,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    std::vector<int> col_indices(matrix.cols());
    std::iota(col_indices.begin(), col_indices.end(), 0);

    auto comp = [&matrix, direction](int i, int j) -> bool {
        return direction == SortingDirection::ASCENDING ? matrix(0, i) < matrix(0, j) : matrix(0, i) > matrix(0, j);
    };

    execute_sorting_algorithm(col_indices.begin(), col_indices.end(), comp, algorithm);

    Eigen::MatrixXd sorted_matrix = matrix;
    for (int j = 0; j < matrix.cols(); ++j) {
        sorted_matrix.col(j) = matrix.col(col_indices[j]);
    }
    matrix = sorted_matrix;
}

Eigen::VectorXd sort_eigen_eigenvalues(const Eigen::MatrixXd& matrix,
    SortingDirection direction)
{
    if (matrix.rows() != matrix.cols()) {
        throw std::invalid_argument("Matrix must be square for eigenvalue computation");
    }

    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(matrix);
    auto eigenvalues = solver.eigenvalues();

    std::vector<double> eig_vec(eigenvalues.data(), eigenvalues.data() + eigenvalues.size());
    sort_container(eig_vec, direction, SortingAlgorithm::STANDARD);

    return Eigen::Map<Eigen::VectorXd>(eig_vec.data(), eig_vec.size());
}

} // namespace MayaFlux::Yantra
