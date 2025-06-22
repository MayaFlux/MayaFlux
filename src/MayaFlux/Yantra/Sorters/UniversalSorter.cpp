#include "UniversalSorter.hpp"

namespace MayaFlux::Yantra {

SorterOutput UniversalSorter::apply_operation(SorterInput input)
{
    if (!check_flow_constraints(input)) {
        throw std::runtime_error("Flow enforcement constraints violated");
    }

    return std::visit([this](const auto& data) -> SorterOutput {
        using InputType = std::decay_t<decltype(data)>;

        if (should_use_analyzer()) {
            return sort_via_analyzer(data);
        }

        if constexpr (std::same_as<InputType, Kakshya::DataVariant>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, Kakshya::Region>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, Kakshya::RegionGroup>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, std::vector<Kakshya::RegionSegment>>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, AnalyzerOutput>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, std::vector<double>>) {
            auto sorted_data = data;
            sort_container(sorted_data, m_direction, m_algorithm);
            return SorterOutput { sorted_data };
        } else if constexpr (std::same_as<InputType, std::vector<float>>) {
            auto sorted_data = data;
            sort_container(sorted_data, m_direction, m_algorithm);
            return SorterOutput { sorted_data };
        } else if constexpr (std::same_as<InputType, std::vector<std::complex<double>>>) {
            auto sorted_data = data;
            sort_complex_container(sorted_data, m_direction, m_algorithm);
            return SorterOutput { sorted_data };
        } else if constexpr (std::same_as<InputType, Eigen::MatrixXd>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, Eigen::VectorXd>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<InputType, std::vector<std::any>>) {
            return this->sort_impl(data);
        } else {
            throw std::runtime_error("Unsupported input type for sorting: " + std::string(typeid(InputType).name()));
        }
    },
        input);
}

SorterOutput UniversalSorter::sort_with_algorithm(SorterInput input, SortingAlgorithm algorithm)
{
    auto old_algorithm = m_algorithm;
    m_algorithm = algorithm;

    auto result = apply_operation(input);

    m_algorithm = old_algorithm;
    return result;
}

std::vector<SorterOutput> UniversalSorter::sort_chunked(SorterInput input, size_t chunk_size)
{
    return std::visit([&](const auto& data) -> std::vector<SorterOutput> {
        using DataType = std::decay_t<decltype(data)>;

        if constexpr (std::same_as<DataType, std::vector<double>>) {
            auto chunks = sort_chunked_standard(data, chunk_size, m_direction, m_algorithm);
            std::vector<SorterOutput> result;
            result.reserve(chunks.size());
            for (const auto& chunk : chunks) {
                result.emplace_back(SorterOutput { std::vector<double>(chunk.begin(), chunk.end()) });
            }
            return result;
        } else if constexpr (std::same_as<DataType, std::vector<float>>) {
            auto chunks = sort_chunked_standard(data, chunk_size, m_direction, m_algorithm);
            std::vector<SorterOutput> result;
            result.reserve(chunks.size());
            for (const auto& chunk : chunks) {
                result.emplace_back(SorterOutput { std::vector<float>(chunk.begin(), chunk.end()) });
            }
            return result;
        } else {
            return { apply_operation(input) };
        }
    },
        input);
}

SorterOutput UniversalSorter::sort_by_grammar(SorterInput input, const std::string& rule_name)
{
    if (auto result = m_grammar.sort_by_rule(rule_name, input)) {
        return *result;
    }

    throw std::runtime_error("Grammar rule not found or doesn't match: " + rule_name);
}

SorterOutput UniversalSorter::sort_impl(const Kakshya::DataVariant& data)
{
    return std::visit([this](const auto& vec) -> SorterOutput {
        using VecType = std::decay_t<decltype(vec)>;

        if constexpr (std::same_as<VecType, std::vector<double>>) {
            auto sorted_vec = vec;
            sort_container(sorted_vec, m_direction, m_algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else if constexpr (std::same_as<VecType, std::vector<float>>) {
            auto sorted_vec = vec;
            sort_container(sorted_vec, m_direction, m_algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else if constexpr (std::same_as<VecType, std::vector<std::complex<double>>>) {
            auto sorted_vec = vec;
            sort_complex_container(sorted_vec, m_direction, m_algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else if constexpr (std::same_as<VecType, std::vector<std::complex<float>>>) {
            auto sorted_vec = vec;
            sort_complex_container(sorted_vec, m_direction, m_algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else if constexpr (std::same_as<VecType, std::vector<unsigned char>>) {
            auto sorted_vec = vec;
            sort_container(sorted_vec, m_direction, m_algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else if constexpr (std::same_as<VecType, std::vector<unsigned short>>) {
            auto sorted_vec = vec;
            sort_container(sorted_vec, m_direction, m_algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else if constexpr (std::same_as<VecType, std::vector<unsigned int>>) {
            auto sorted_vec = vec;
            sort_container(sorted_vec, m_direction, m_algorithm);
            return Kakshya::DataVariant { sorted_vec };
        } else {
            throw std::runtime_error("Unsupported data type for sorting: " + std::string(typeid(VecType).name()));
        }
    },
        data);
}

SorterOutput UniversalSorter::sort_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container is null or has no data");
    }

    auto data = container->get_processed_data();
    return sort_impl(data);
}

SorterOutput UniversalSorter::sort_impl(const Kakshya::Region& region)
{
    Kakshya::RegionGroup group;
    group.regions.push_back(region);
    group.name = "single_region_group";
    return group;
}

SorterOutput UniversalSorter::sort_impl(const Kakshya::RegionGroup& group)
{
    auto sorted_group = group;
    sort_regions(sorted_group.regions, m_direction, m_algorithm);
    return sorted_group;
}

SorterOutput UniversalSorter::sort_impl(const std::vector<Kakshya::RegionSegment>& segments)
{
    auto sorted_segments = segments;
    sort_segments(sorted_segments, m_direction, m_algorithm);
    return sorted_segments;
}

SorterOutput UniversalSorter::sort_impl(const AnalyzerOutput& output)
{
    return std::visit([this](const auto& data) -> SorterOutput {
        using AnalyzerDataType = std::decay_t<decltype(data)>;

        if constexpr (std::same_as<AnalyzerDataType, std::vector<double>>) {
            auto sorted_data = data;
            sort_container(sorted_data, m_direction, m_algorithm);
            return SorterOutput { sorted_data };
        } else if constexpr (std::same_as<AnalyzerDataType, Kakshya::DataVariant>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<AnalyzerDataType, Kakshya::RegionGroup>) {
            return this->sort_impl(data);
        } else if constexpr (std::same_as<AnalyzerDataType, std::vector<Kakshya::RegionSegment>>) {
            return this->sort_impl(data);
        } else {
            throw std::runtime_error("Unsupported AnalyzerOutput type for sorting: " + std::string(typeid(AnalyzerDataType).name()));
        }
    },
        output);
}

SorterOutput UniversalSorter::sort_impl(const Eigen::MatrixXd& matrix)
{
    std::string sort_method = get_sorting_method();

    if (sort_method == "eigenvalues") {
        if (matrix.rows() == matrix.cols()) {
            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(matrix);
            auto eigenvalues = solver.eigenvalues();

            std::vector<double> eig_vec(eigenvalues.data(), eigenvalues.data() + eigenvalues.size());
            return sort_impl(Kakshya::DataVariant { eig_vec });
        }
    } else if (sort_method == "by_rows") {
        auto sorted_matrix = matrix;
        std::vector<int> row_indices(matrix.rows());
        std::iota(row_indices.begin(), row_indices.end(), 0);

        auto comparator = create_standard_comparator<double>(m_direction);
        std::sort(row_indices.begin(), row_indices.end(), [&](int i, int j) {
            return comparator(matrix(i, 0), matrix(j, 0));
        });

        for (int i = 0; i < matrix.rows(); ++i) {
            sorted_matrix.row(i) = matrix.row(row_indices[i]);
        }
        return sorted_matrix;
    } else if (sort_method == "by_columns") {
        auto sorted_matrix = matrix;
        std::vector<int> col_indices(matrix.cols());
        std::iota(col_indices.begin(), col_indices.end(), 0);

        auto comparator = create_standard_comparator<double>(m_direction);
        std::sort(col_indices.begin(), col_indices.end(), [&](int i, int j) {
            return comparator(matrix(0, i), matrix(0, j));
        });

        for (int j = 0; j < matrix.cols(); ++j) {
            sorted_matrix.col(j) = matrix.col(col_indices[j]);
        }
        return sorted_matrix;
    }

    std::vector<double> flat_data(matrix.data(), matrix.data() + matrix.size());
    return sort_impl(Kakshya::DataVariant { flat_data });
}

SorterOutput UniversalSorter::sort_impl(const Eigen::VectorXd& vector)
{
    std::vector<double> data(vector.data(), vector.data() + vector.size());
    auto sorted_data = data;
    sort_container(sorted_data, m_direction, m_algorithm);

    Eigen::VectorXd result = Eigen::Map<Eigen::VectorXd>(sorted_data.data(), sorted_data.size());
    return result;
}

SorterOutput UniversalSorter::sort_impl(const std::vector<std::any>& data)
{
    std::vector<size_t> indices(data.size());
    std::iota(indices.begin(), indices.end(), 0);

    if (!data.empty()) {
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            const auto& val_a = data[a];
            const auto& val_b = data[b];

            if (val_a.type() == val_b.type()) {
                if (val_a.type() == typeid(double)) {
                    double a_val = std::any_cast<double>(val_a);
                    double b_val = std::any_cast<double>(val_b);
                    return m_direction == SortDirection::ASCENDING ? a_val < b_val : a_val > b_val;
                } else if (val_a.type() == typeid(float)) {
                    float a_val = std::any_cast<float>(val_a);
                    float b_val = std::any_cast<float>(val_b);
                    return m_direction == SortDirection::ASCENDING ? a_val < b_val : a_val > b_val;
                }
            }

            return a < b;
        });
    }

    return indices;
}

bool UniversalSorter::should_use_analyzer() const
{
    if (!m_use_analyzer || !m_analyzer) {
        return false;
    }

    std::string method = get_sorting_method();
    return requires_analyzer_delegation(method);
}

SorterOutput UniversalSorter::convert_from_analyzer_output(const AnalyzerOutput& output)
{
    return std::visit([this](const auto& result) -> SorterOutput {
        using T = std::decay_t<decltype(result)>;

        if constexpr (std::is_same_v<T, std::vector<double>>) {
            auto sorted_result = result;
            sort_container(sorted_result, m_direction, m_algorithm);
            return SorterOutput { sorted_result };
        } else if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            return SorterOutput { result };
        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
            return SorterOutput { result };
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            if (result && result->has_data()) {
                auto data = result->get_processed_data();
                return sort_impl(data);
            }
            return SorterOutput { std::vector<double> {} };
        } else if constexpr (std::is_same_v<T, Kakshya::Region>) {
            return sort_impl(result);
        } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
            return sort_impl(result);
        } else {
            throw std::runtime_error("Cannot convert AnalyzerOutput type to SorterOutput");
        }
    },
        output);
}

bool UniversalSorter::requires_analyzer_delegation(const std::string& method) const
{
    static const std::unordered_set<std::string> analyzer_methods = {
        "statistical", "percentile", "distribution", "outlier_aware",
        "mean_based", "variance_based", "entropy_based", "correlation_based"
    };

    return analyzer_methods.count(method) > 0;
}

std::vector<std::string> UniversalSorter::get_methods_for_type(std::type_index type_info) const
{
    return get_methods_for_type_impl(type_info);
}

SorterOutput UniversalSorter::format_output_based_on_granularity(const SorterOutput& raw_output) const
{
    switch (m_granularity) {
    case SortingGranularity::INDICES_ONLY: {
        return std::visit([](const auto& data) -> SorterOutput {
            using DataType = std::decay_t<decltype(data)>;

            if constexpr (std::same_as<DataType, std::vector<size_t>>) {
                return data;
            } else if constexpr (std::ranges::range<DataType>) {
                std::vector<size_t> indices(data.size());
                std::iota(indices.begin(), indices.end(), 0);
                return indices;
            } else {
                return std::vector<size_t> {};
            }
        },
            raw_output);
    }

    case SortingGranularity::SORTED_VALUES:
        return raw_output;

    case SortingGranularity::ATTRIBUTED_SEGMENTS: {
        return std::visit([](const auto& data) -> SorterOutput {
            using DataType = std::decay_t<decltype(data)>;

            if constexpr (std::same_as<DataType, std::vector<Kakshya::RegionSegment>>) {
                return data;
            } else {
                return std::vector<Kakshya::RegionSegment> {};
            }
        },
            raw_output);
    }

    case SortingGranularity::ORGANIZED_GROUPS: {
        return std::visit([](const auto& data) -> SorterOutput {
            using DataType = std::decay_t<decltype(data)>;

            if constexpr (std::same_as<DataType, Kakshya::RegionGroup>) {
                return data;
            } else {
                return Kakshya::RegionGroup {};
            }
        },
            raw_output);
    }

    case SortingGranularity::MULTI_DIMENSIONAL:
        return raw_output;

    default:
        return raw_output;
    }
}

bool UniversalSorter::check_flow_constraints(const SorterInput& input) const
{
    if (!m_flow_enforcement) {
        return true;
    }

    return std::visit([](const auto& data) -> bool {
        using DataType = std::decay_t<decltype(data)>;

        if constexpr (std::same_as<DataType, AnalyzerOutput>) {
            return true;
        } else if constexpr (std::same_as<DataType, std::vector<Kakshya::RegionSegment>>) {
            return true;
        } else {
            return false;
        }
    },
        input);
}

std::string UniversalSorter::get_sorting_method() const
{
    try {
        return std::any_cast<std::string>(get_parameter("method"));
    } catch (const std::bad_any_cast&) {
        try {
            return std::string(std::any_cast<const char*>(get_parameter("method")));
        } catch (const std::bad_any_cast&) {
            return "default";
        } catch (const std::runtime_error&) {
            return "default";
        }
    } catch (const std::runtime_error&) {
        return "default";
    }
}

std::vector<std::string> UniversalSorter::get_methods_for_type_impl(std::type_index type_info) const
{
    return { "default", "ascending", "descending" };
}

void UniversalSorter::set_parameter(const std::string& name, std::any value)
{
    m_parameters[name] = std::move(value);
}

std::any UniversalSorter::get_parameter(const std::string& name) const
{
    return Utils::safe_get_parameter(name, m_parameters);
}

std::map<std::string, std::any> UniversalSorter::get_all_parameters() const
{
    return m_parameters;
}

SorterOutput UniversalSorter::sort_multi_key(SorterInput input, const std::vector<SortKey>& keys)
{
    if (keys.empty()) {
        return apply_operation(input);
    }

    return std::visit([&](const auto& data) -> SorterOutput {
        using DataType = std::decay_t<decltype(data)>;

        if constexpr (std::same_as<DataType, std::vector<std::any>>) {
            auto sorted_data = data;
            std::vector<size_t> indices(data.size());
            std::iota(indices.begin(), indices.end(), 0);

            std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
                for (const auto& key : keys) {
                    try {
                        double val_a = key.extractor(data[a]);
                        double val_b = key.extractor(data[b]);

                        if (key.normalize) {
                            // TODO: Implement normalization if needed
                        }

                        double weighted_diff = key.weight * (val_a - val_b);
                        if (std::abs(weighted_diff) > 1e-9) {
                            return key.direction == SortDirection::ASCENDING ? weighted_diff < 0 : weighted_diff > 0;
                        }
                    } catch (...) {
                        continue;
                    }
                }
                return a < b;
            });

            std::vector<std::any> result;
            result.reserve(data.size());
            for (size_t idx : indices) {
                result.push_back(data[idx]);
            }
            return result;
        } else {
            return apply_operation(input);
        }
    },
        input);
}

void SortingGrammar::add_rule(const Rule& rule)
{
    m_rules.push_back(rule);

    std::sort(m_rules.begin(), m_rules.end(), [](const Rule& a, const Rule& b) {
        return a.priority > b.priority;
    });
}

std::optional<SorterOutput> SortingGrammar::sort_by_rule(const std::string& rule_name, const SorterInput& input) const
{
    auto it = std::find_if(m_rules.begin(), m_rules.end(), [&](const Rule& rule) {
        return rule.name == rule_name && rule.matcher(input);
    });

    if (it != m_rules.end()) {
        return it->sorter(input);
    }

    return std::nullopt;
}

std::vector<SorterOutput> SortingGrammar::sort_all_matching(const SorterInput& input) const
{
    std::vector<SorterOutput> results;

    for (const auto& rule : m_rules) {
        if (rule.matcher(input)) {
            results.push_back(rule.sorter(input));
        }
    }

    return results;
}

std::vector<std::string> SortingGrammar::get_available_rules() const
{
    std::vector<std::string> names;
    names.reserve(m_rules.size());

    std::transform(m_rules.begin(), m_rules.end(), std::back_inserter(names),
        [](const Rule& rule) { return rule.name; });

    return names;
}

} // namespace MayaFlux::Yantra
