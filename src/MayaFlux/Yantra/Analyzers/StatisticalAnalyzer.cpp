#include "StatisticalAnalyzer.hpp"
#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

namespace MayaFlux::Yantra {

StatisticalAnalyzer::StatisticalAnalyzer()
{
    UniversalAnalyzer::set_parameter("sample_variance", true);
    UniversalAnalyzer::set_parameter("percentile", 50.0);
    UniversalAnalyzer::set_parameter("precision", 1e-10);
}

std::vector<std::string> StatisticalAnalyzer::get_available_methods() const
{
    return {
        method_to_string(Method::MEAN),
        method_to_string(Method::VARIANCE),
        method_to_string(Method::STD_DEV),
        method_to_string(Method::SKEWNESS),
        method_to_string(Method::KURTOSIS),
        method_to_string(Method::MIN),
        method_to_string(Method::MAX),
        method_to_string(Method::MEDIAN),
        method_to_string(Method::RANGE),
        method_to_string(Method::PERCENTILE),
        method_to_string(Method::MODE),
        method_to_string(Method::MAD),
        method_to_string(Method::CV),
        method_to_string(Method::SUM),
        method_to_string(Method::COUNT),
        method_to_string(Method::RMS)
    };
}

std::vector<std::string> StatisticalAnalyzer::get_methods_for_type_impl(std::type_index type_info) const
{
    return get_available_methods();
}

AnalyzerOutput StatisticalAnalyzer::analyze_impl(const Kakshya::DataVariant& data)
{
    const std::string method = get_analysis_method();
    const Method stat_method = string_to_method(method);

    auto numeric_data_opt = Kakshya::extract_from_variant<double>(data);
    if (!numeric_data_opt.has_value()) {
        auto float_data_opt = Kakshya::extract_from_variant<float>(data);
        if (float_data_opt.has_value()) {
            std::vector<double> numeric_data(float_data_opt->begin(), float_data_opt->end());
            validate_data(numeric_data, stat_method);
            double result = calculate_statistic_for_method(numeric_data, stat_method);
            std::vector<double> result_vector = { result };
            return format_output_based_on_granularity(result_vector, method);
        }
        throw std::runtime_error("Cannot extract numeric data from DataVariant");
    }

    std::vector<double> numeric_data = numeric_data_opt.value();
    validate_data(numeric_data, stat_method);

    double result = calculate_statistic_for_method(numeric_data, stat_method);
    std::vector<double> result_vector = { result };
    return format_output_based_on_granularity(result_vector, method);
}

AnalyzerOutput StatisticalAnalyzer::analyze_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container is null or has no data");
    }

    auto dimensions = container->get_dimensions();
    if (dimensions.empty()) {
        throw std::runtime_error("Container has no dimensions");
    }

    const std::string method = get_analysis_method();
    const Method stat_method = string_to_method(method);

    DataModality modality = detect_data_modality(dimensions);

    const Kakshya::DataVariant& container_data = container->get_processed_data();

    auto numeric_data_opt = Kakshya::extract_from_variant<double>(container_data);
    if (!numeric_data_opt.has_value()) {
        auto float_data_opt = Kakshya::extract_from_variant<float>(container_data);
        if (!float_data_opt.has_value()) {
            throw std::runtime_error("Cannot extract numeric data from container");
        }
        std::vector<double> numeric_data(float_data_opt->begin(), float_data_opt->end());
        validate_data(numeric_data, stat_method);

        std::vector<double> results = process_by_modality(numeric_data, dimensions, modality, stat_method);
        return format_output_based_on_granularity(results, method);
    }

    std::vector<double> numeric_data = numeric_data_opt.value();
    validate_data(numeric_data, stat_method);

    std::vector<double> results = process_by_modality(numeric_data, dimensions, modality, stat_method);
    return format_output_based_on_granularity(results, method);
}

AnalyzerOutput StatisticalAnalyzer::analyze_impl(const Kakshya::Region& region)
{
    auto it = region.attributes.find("data");
    if (it != region.attributes.end()) {
        if (it->second.type() == typeid(std::vector<double>)) {
            const auto& data = std::any_cast<const std::vector<double>&>(it->second);
            const std::string method = get_analysis_method();
            const Method stat_method = string_to_method(method);
            validate_data(data, stat_method);
            double result = calculate_statistic_for_method(data, stat_method);
            std::vector<double> result_vector = { result };
            return format_output_based_on_granularity(result_vector, method);
        }
        if (it->second.type() == typeid(std::vector<float>)) {
            const auto& data = std::any_cast<const std::vector<float>&>(it->second);
            std::vector<double> ddata(data.begin(), data.end());
            const std::string method = get_analysis_method();
            const Method stat_method = string_to_method(method);
            validate_data(ddata, stat_method);
            double result = calculate_statistic_for_method(ddata, stat_method);
            std::vector<double> result_vector = { result };
            return format_output_based_on_granularity(result_vector, method);
        }
    }

    auto container_param = get_parameter("current_container");
    if (!container_param.has_value()) {
        throw std::runtime_error("Region analysis requires container context. Call set_parameter(\"current_container\", container) first.");
    }

    auto container = std::any_cast<std::shared_ptr<Kakshya::SignalSourceContainer>>(container_param);
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container context is invalid");
    }

    auto region_data = container->get_region_data(region);
    return analyze_impl(region_data);
}

AnalyzerOutput StatisticalAnalyzer::analyze_impl(const Kakshya::RegionGroup& group)
{
    const std::string method = get_analysis_method();
    const Method stat_method = string_to_method(method);

    bool all_have_data = true;
    for (const auto& region : group.regions) {
        if (region.attributes.find("data") == region.attributes.end()) {
            all_have_data = false;
            break;
        }
    }
    if (all_have_data && !group.regions.empty()) {
        std::vector<double> results;
        for (const auto& region : group.regions) {
            const auto& attr = region.attributes.at("data");
            if (attr.type() == typeid(std::vector<double>)) {
                const auto& data = std::any_cast<const std::vector<double>&>(attr);
                validate_data(data, stat_method);
                results.push_back(calculate_statistic_for_method(data, stat_method));
            } else if (attr.type() == typeid(std::vector<float>)) {
                const auto& data = std::any_cast<const std::vector<float>&>(attr);
                std::vector<double> ddata(data.begin(), data.end());
                validate_data(ddata, stat_method);
                results.push_back(calculate_statistic_for_method(ddata, stat_method));
            }
        }
        return format_output_based_on_granularity(results, method);
    }

    auto container_param = get_parameter("current_container");
    if (!container_param.has_value()) {
        std::vector<double> results;

        for (const auto& region : group.regions) {
            if (!region.start_coordinates.empty() && !region.end_coordinates.empty()) {
                double span = static_cast<double>(region.get_span(0));
                results.push_back(span);
            }
        }

        if (results.empty()) {
            throw std::runtime_error("RegionGroup analysis requires container context or regions with coordinate data");
        }

        return format_output_based_on_granularity(results, method);
    }

    auto container = std::any_cast<std::shared_ptr<Kakshya::SignalSourceContainer>>(container_param);
    std::vector<double> all_data;

    for (const auto& region : group.regions) {
        try {
            auto region_data = container->get_region_data(region);
            auto numeric_opt = Kakshya::extract_from_variant<double>(region_data);
            if (numeric_opt.has_value()) {
                all_data.insert(all_data.end(), numeric_opt->begin(), numeric_opt->end());
            }
        } catch (const std::exception& e) {
            continue;
        }
    }

    if (all_data.empty()) {
        throw std::runtime_error("No data could be extracted from regions in group");
    }

    validate_data(all_data, stat_method);
    double result = calculate_statistic_for_method(all_data, stat_method);
    std::vector<double> results = { result };

    return format_output_based_on_granularity(results, method);
}

AnalyzerOutput StatisticalAnalyzer::analyze_impl(const std::vector<Kakshya::RegionSegment>& segments)
{
    const std::string method = get_analysis_method();
    const Method stat_method = string_to_method(method);

    std::vector<double> results;

    for (const auto& segment : segments) {
        // if (segment.is_cached && std::holds_alternative<Kakshya::DataVariant>(segment.cache.data)) {
        if (segment.is_cached) {
            auto numeric_opt = Kakshya::extract_from_variant<double>(segment.cache.data);
            if (numeric_opt.has_value()) {
                validate_data(numeric_opt.value(), stat_method);
                double result = calculate_statistic_for_method(numeric_opt.value(), stat_method);
                results.push_back(result);
            }
        } else {
            auto container_param = get_parameter("current_container");
            if (container_param.has_value()) {
                auto container = std::any_cast<std::shared_ptr<Kakshya::SignalSourceContainer>>(container_param);
                try {
                    auto region_data = container->get_region_data(segment.source_region);
                    auto numeric_opt = Kakshya::extract_from_variant<double>(region_data);
                    if (numeric_opt.has_value()) {
                        validate_data(numeric_opt.value(), stat_method);
                        double result = calculate_statistic_for_method(numeric_opt.value(), stat_method);
                        results.push_back(result);
                    }
                } catch (const std::exception& e) {
                    continue;
                }
            }
        }
    }

    if (results.empty()) {
        throw std::runtime_error("No segments have analyzable data");
    }

    return format_output_based_on_granularity(results, method);
}

double StatisticalAnalyzer::calculate_statistic_for_method(const std::vector<double>& data, Method method)
{
    switch (method) {
    case Method::MEAN:
        return calculate_mean(data);
    case Method::VARIANCE:
        return calculate_variance(data, get_parameter_or_default<bool>("sample_variance", true));
    case Method::STD_DEV:
        return calculate_std_dev(data, get_parameter_or_default<bool>("sample_variance", true));
    case Method::SKEWNESS:
        return calculate_skewness(data);
    case Method::KURTOSIS:
        return calculate_kurtosis(data);
    case Method::MIN:
        return *std::ranges::min_element(data);
    case Method::MAX:
        return *std::ranges::max_element(data);
    case Method::MEDIAN:
        return calculate_median(data);
    case Method::RANGE:
        return *std::ranges::max_element(data) - *std::ranges::min_element(data);
    case Method::PERCENTILE:
        return calculate_percentile(data, get_parameter_or_default<double>("percentile", 50.0));
    case Method::MODE:
        return calculate_mode(data);
    case Method::MAD:
        return calculate_mad(data);
    case Method::CV:
        return calculate_cv(data);
    case Method::SUM:
        return std::accumulate(data.begin(), data.end(), 0.0);
    case Method::COUNT:
        return static_cast<double>(data.size());
    case Method::RMS:
        return calculate_rms(data);
    default:
        throw std::invalid_argument("Unknown statistical method");
    }
}

double StatisticalAnalyzer::calculate_mean(const std::vector<double>& data)
{
    if (data.empty()) {
        throw std::invalid_argument("Cannot calculate mean of empty dataset");
    }

    Eigen::Map<const Eigen::VectorXd> eigen_data(data.data(), data.size());
    return eigen_data.mean();
}

double StatisticalAnalyzer::calculate_variance(const std::vector<double>& data, bool sample)
{
    if (data.size() < 2) {
        throw std::invalid_argument("Variance requires at least 2 data points");
    }

    double mean = calculate_mean(data);

    Eigen::Map<const Eigen::VectorXd> eigen_data(data.data(), data.size());
    Eigen::VectorXd diff = eigen_data.array() - mean;
    double sum_sq_diff = diff.array().square().sum();

    int denominator = sample ? (data.size() - 1) : data.size();
    return sum_sq_diff / denominator;
}

double StatisticalAnalyzer::calculate_std_dev(const std::vector<double>& data, bool sample)
{
    return std::sqrt(calculate_variance(data, sample));
}

double StatisticalAnalyzer::calculate_skewness(const std::vector<double>& data)
{
    if (data.size() < 3) {
        throw std::invalid_argument("Skewness requires at least 3 data points");
    }

    double mean = calculate_mean(data);
    double std_dev = calculate_std_dev(data, true);

    if (std_dev == 0.0) {
        return 0.0;
    }

    Eigen::Map<const Eigen::VectorXd> eigen_data(data.data(), data.size());
    Eigen::VectorXd standardized = (eigen_data.array() - mean) / std_dev;
    double skewness = standardized.array().cube().mean();

    double n = static_cast<double>(data.size());
    return skewness * std::sqrt(n * (n - 1)) / (n - 2);
}

double StatisticalAnalyzer::calculate_kurtosis(const std::vector<double>& data)
{
    if (data.size() < 4) {
        throw std::invalid_argument("Kurtosis requires at least 4 data points");
    }

    double mean = calculate_mean(data);
    double std_dev = calculate_std_dev(data, true);

    if (std_dev == 0.0) {
        return 0.0;
    }

    Eigen::Map<const Eigen::VectorXd> eigen_data(data.data(), data.size());
    Eigen::VectorXd standardized = (eigen_data.array() - mean) / std_dev;
    double kurtosis = standardized.array().pow(4).mean();

    double n = static_cast<double>(data.size());
    double correction = (n - 1) * ((n + 1) * kurtosis - 3 * (n - 1)) / ((n - 2) * (n - 3));
    return correction;
}

double StatisticalAnalyzer::calculate_percentile(const std::vector<double>& data, double percentile)
{
    if (data.empty()) {
        throw std::invalid_argument("Cannot calculate percentile of empty dataset");
    }

    if (percentile < 0.0 || percentile > 100.0) {
        throw std::out_of_range("Percentile must be between 0 and 100");
    }

    std::vector<double> sorted_data = data;
    std::sort(sorted_data.begin(), sorted_data.end());

    if (percentile == 0.0)
        return sorted_data.front();
    if (percentile == 100.0)
        return sorted_data.back();

    double index = (percentile / 100.0) * (sorted_data.size() - 1);
    size_t lower_index = static_cast<size_t>(std::floor(index));
    size_t upper_index = static_cast<size_t>(std::ceil(index));

    if (lower_index == upper_index) {
        return sorted_data[lower_index];
    }

    double weight = index - lower_index;
    return sorted_data[lower_index] * (1.0 - weight) + sorted_data[upper_index] * weight;
}

double StatisticalAnalyzer::calculate_median(const std::vector<double>& data)
{
    return calculate_percentile(data, 50.0);
}

double StatisticalAnalyzer::calculate_mode(const std::vector<double>& data)
{
    if (data.empty()) {
        throw std::invalid_argument("Cannot calculate mode of empty dataset");
    }

    std::unordered_map<double, int> frequency_map;
    for (double value : data) {
        frequency_map[value]++;
    }

    auto max_freq_pair = std::max_element(frequency_map.begin(), frequency_map.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return max_freq_pair->first;
}

double StatisticalAnalyzer::calculate_mad(const std::vector<double>& data)
{
    if (data.empty()) {
        throw std::invalid_argument("Cannot calculate MAD of empty dataset");
    }

    double median = calculate_median(data);

    std::vector<double> absolute_deviations;
    absolute_deviations.reserve(data.size());

    for (double value : data) {
        absolute_deviations.push_back(std::abs(value - median));
    }

    return calculate_median(absolute_deviations);
}

double StatisticalAnalyzer::calculate_cv(const std::vector<double>& data)
{
    double mean = calculate_mean(data);
    if (mean == 0.0) {
        throw std::runtime_error("Cannot calculate coefficient of variation when mean is zero");
    }

    double std_dev = calculate_std_dev(data, true);
    return std_dev / std::abs(mean);
}

double StatisticalAnalyzer::calculate_rms(const std::vector<double>& data)
{
    if (data.empty()) {
        throw std::invalid_argument("Cannot calculate RMS of empty dataset");
    }

    Eigen::Map<const Eigen::VectorXd> eigen_data(data.data(), data.size());
    return std::sqrt(eigen_data.array().square().mean());
}

void StatisticalAnalyzer::validate_data(const std::vector<double>& data, Method method)
{
    if (data.empty()) {
        throw std::invalid_argument("Cannot perform statistical analysis on empty data");
    }

    switch (method) {
    case Method::VARIANCE:
    case Method::STD_DEV:
        if (data.size() < 2) {
            throw std::invalid_argument("Variance and standard deviation require at least 2 data points");
        }
        break;
    case Method::SKEWNESS:
        if (data.size() < 3) {
            throw std::invalid_argument("Skewness requires at least 3 data points");
        }
        break;
    case Method::KURTOSIS:
        if (data.size() < 4) {
            throw std::invalid_argument("Kurtosis requires at least 4 data points");
        }
        break;
    default:
        break;
    }

    for (double value : data) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("Data contains NaN or infinite values");
        }
    }
}

DataModality StatisticalAnalyzer::detect_data_modality(const std::vector<Kakshya::DataDimension>& dimensions)
{
    if (dimensions.empty()) {
        return DataModality::UNKNOWN;
    }

    if (dimensions.size() == 1) {
        return DataModality::AUDIO_1D;
    } else if (dimensions.size() == 2) {
        return DataModality::SPECTRAL_2D;
    } else if (dimensions.size() == 3) {
        return DataModality::VOLUMETRIC_3D;
    }

    return DataModality::TENSOR_ND;
}

std::vector<double> StatisticalAnalyzer::process_by_modality(
    const std::vector<double>& data,
    const std::vector<Kakshya::DataDimension>& dimensions,
    DataModality /*modality*/,
    Method method)
{
    size_t time_frames = 0;
    size_t freq_bins = 0;
    for (const auto& dim : dimensions) {
        if (dim.role == Kakshya::DataDimension::Role::TIME) {
            time_frames = dim.size;
        }
        if (dim.role == Kakshya::DataDimension::Role::FREQUENCY) {
            freq_bins = dim.size;
        }
    }

    if (freq_bins > 1 && time_frames > 0) {
        std::vector<double> results(freq_bins, 0.0);
        for (size_t freq = 0; freq < freq_bins; ++freq) {
            std::vector<double> freq_data;
            for (size_t time = 0; time < time_frames; ++time) {
                size_t index = time * freq_bins + freq;
                if (index < data.size()) {
                    freq_data.push_back(data[index]);
                }
            }
            if (!freq_data.empty()) {
                results[freq] = calculate_statistic_for_method(freq_data, method);
            }
        }
        return results;
    }

    double result = calculate_statistic_for_method(data, method);
    return { result };
}

AnalyzerOutput StatisticalAnalyzer::format_output_based_on_granularity(const std::vector<double>& values, const std::string& method)
{
    AnalysisGranularity granularity = get_output_granularity();

    switch (granularity) {
    case AnalysisGranularity::RAW_VALUES:
        return AnalyzerOutput { values };

    case AnalysisGranularity::ATTRIBUTED_SEGMENTS:
        return AnalyzerOutput { create_statistical_segments(values, method) };

    case AnalysisGranularity::ORGANIZED_GROUPS:
        return AnalyzerOutput { create_statistical_regions(values, method) };

    default:
        return AnalyzerOutput { values };
    }
}

Kakshya::RegionGroup StatisticalAnalyzer::create_statistical_regions(const std::vector<double>& values, const std::string& method)
{
    Kakshya::RegionGroup group;
    group.name = std::string("Statistical Analysis - " + method);
    group.attributes["description"] = std::string("Statistical regions based on " + method + " analysis");

    if (!values.empty()) {
        double mean_val = calculate_mean(values);
        double std_val = values.size() > 1 ? calculate_std_dev(values, true) : 0.0;

        for (size_t i = 0; i < values.size(); ++i) {
            Kakshya::Region region;
            region.start_coordinates = { static_cast<u_int64_t>(i) };
            region.end_coordinates = { static_cast<u_int64_t>(i + 1) };

            region.attributes["value"] = values[i];

            std::string classification;
            if (std_val > 0.0) {
                double z_score = (values[i] - mean_val) / std_val;
                if (z_score > 2.0) {
                    classification = "high_" + method;
                } else if (z_score < -2.0) {
                    classification = "low_" + method;
                } else {
                    classification = "normal_" + method;
                }
                region.attributes["z_score"] = z_score;
            } else {
                classification = "constant_" + method;
            }

            region.attributes["classification"] = classification;

            group.regions.push_back(region);
        }
    }

    return group;
}

std::vector<Kakshya::RegionSegment> StatisticalAnalyzer::create_statistical_segments(const std::vector<double>& values, const std::string& method)
{
    std::vector<Kakshya::RegionSegment> segments;

    for (size_t i = 0; i < values.size(); ++i) {
        Kakshya::RegionSegment segment;

        segment.source_region.start_coordinates = { static_cast<u_int64_t>(i) };
        segment.source_region.end_coordinates = { static_cast<u_int64_t>(i + 1) };

        segment.source_region.attributes["method"] = method;
        segment.source_region.attributes["value"] = values[i];
        segment.source_region.attributes["index"] = static_cast<int>(i);

        segments.push_back(segment);
    }

    return segments;
}

std::string StatisticalAnalyzer::method_to_string(Method method)
{
    switch (method) {
    case Method::MEAN:
        return "mean";
    case Method::VARIANCE:
        return "variance";
    case Method::STD_DEV:
        return "std_dev";
    case Method::SKEWNESS:
        return "skewness";
    case Method::KURTOSIS:
        return "kurtosis";
    case Method::MIN:
        return "min";
    case Method::MAX:
        return "max";
    case Method::MEDIAN:
        return "median";
    case Method::RANGE:
        return "range";
    case Method::PERCENTILE:
        return "percentile";
    case Method::MODE:
        return "mode";
    case Method::MAD:
        return "mad";
    case Method::CV:
        return "cv";
    case Method::SUM:
        return "sum";
    case Method::COUNT:
        return "count";
    case Method::RMS:
        return "rms";
    default:
        throw std::invalid_argument("Unknown statistical method enum value");
    }
}

StatisticalAnalyzer::Method StatisticalAnalyzer::string_to_method(const std::string& str)
{
    static const std::unordered_map<std::string, Method> method_map = {
        { "mean", Method::MEAN },
        { "variance", Method::VARIANCE },
        { "std_dev", Method::STD_DEV },
        { "skewness", Method::SKEWNESS },
        { "kurtosis", Method::KURTOSIS },
        { "min", Method::MIN },
        { "max", Method::MAX },
        { "median", Method::MEDIAN },
        { "range", Method::RANGE },
        { "percentile", Method::PERCENTILE },
        { "mode", Method::MODE },
        { "mad", Method::MAD },
        { "cv", Method::CV },
        { "sum", Method::SUM },
        { "count", Method::COUNT },
        { "rms", Method::RMS }
    };

    auto it = method_map.find(str);
    if (it == method_map.end()) {
        throw std::invalid_argument("Unknown statistical method: " + str);
    }

    return it->second;
}

Eigen::VectorXd StatisticalAnalyzer::calculate_matrix_statistics(const Eigen::MatrixXd& matrix, Method method)
{
    switch (method) {
    case Method::MEAN:
        return matrix.colwise().mean();

    case Method::VARIANCE: {
        bool sample = get_parameter_or_default<bool>("sample_variance", true);
        Eigen::MatrixXd centered = matrix.rowwise() - matrix.colwise().mean();
        double divisor = sample ? (matrix.rows() - 1) : matrix.rows();
        return (centered.array().square().colwise().sum() / divisor).matrix();
    }

    case Method::STD_DEV: {
        auto variance = calculate_matrix_statistics(matrix, Method::VARIANCE);
        return variance.array().sqrt();
    }

    case Method::MIN:
        return matrix.colwise().minCoeff();

    case Method::MAX:
        return matrix.colwise().maxCoeff();

    case Method::RANGE: {
        auto min_vals = matrix.colwise().minCoeff();
        auto max_vals = matrix.colwise().maxCoeff();
        return max_vals - min_vals;
    }

    case Method::SUM:
        return matrix.colwise().sum();

    case Method::COUNT:
        return Eigen::VectorXd::Constant(matrix.cols(), matrix.rows());

    case Method::RMS: {
        return (matrix.array().square().colwise().mean()).sqrt();
    }

    default:
        throw std::invalid_argument("Matrix statistics not supported for method: " + method_to_string(method));
    }
}

} // namespace MayaFlux::Yantra
