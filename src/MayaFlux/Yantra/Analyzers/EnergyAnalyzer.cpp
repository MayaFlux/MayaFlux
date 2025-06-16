#include "EnergyAnalyzer.hpp"
#include "AnalysisHelpers.hpp"

#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

#include "unsupported/Eigen/FFT"
#include <Eigen/Geometry>

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#else
#include "execution"
#endif

namespace MayaFlux::Yantra {

EnergyAnalyzer::EnergyAnalyzer(uint32_t window_size, uint32_t hop_size)
    : m_window_size(window_size)
    , m_hop_size(hop_size)
{
    if (m_window_size == 0 || m_hop_size == 0) {
        throw std::invalid_argument("Window size and hop size must be greater than 0");
    }

    set_parameter("window_function", std::string("hanning"));
    set_parameter("overlap_add", true);
    set_parameter("zero_padding", false);
}

std::vector<std::string> EnergyAnalyzer::get_available_methods() const
{
    return {
        method_to_string(Method::RMS),
        method_to_string(Method::PEAK),
        method_to_string(Method::SPECTRAL),
        method_to_string(Method::ZERO_CROSSING),
        method_to_string(Method::HARMONIC),
        method_to_string(Method::POWER),
        method_to_string(Method::DYNAMIC_RANGE)
    };
}

std::vector<std::string> EnergyAnalyzer::get_methods_for_type_impl(std::type_index type_info) const
{
    // All methods support all input types for this analyzer
    return get_available_methods();
}

AnalyzerOutput EnergyAnalyzer::analyze_impl(const Kakshya::DataVariant& data)
{
    const std::string method = get_analysis_method();
    const Method energy_method = string_to_method(method);

    std::vector<double> audio_data = Kakshya::convert_variant_to_double(data);
    std::vector<double> energy_values = calculate_energy_for_method(audio_data, energy_method);

    return format_output_based_on_granularity(energy_values, method);
}

AnalyzerOutput EnergyAnalyzer::analyze_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container is null or has no data");
    }

    // Get dimensions to understand data structure
    auto dimensions = container->get_dimensions();
    if (dimensions.empty()) {
        throw std::runtime_error("Container has no dimensions");
    }

    // Detect data modality based on dimensions
    DataModality modality = detect_data_modality(dimensions);

    // Only handle audio data - delegate others
    if (modality != DataModality::AUDIO_1D && modality != DataModality::AUDIO_MULTICHANNEL && modality != DataModality::SPECTRAL_2D) {
        throw std::runtime_error("EnergyAnalyzer only handles audio and spectral data. Use specialized analyzers for images/video.");
    }

    // Create region covering entire container
    Kakshya::Region full_region;
    full_region.start_coordinates.resize(dimensions.size(), 0);
    full_region.end_coordinates.resize(dimensions.size());

    for (size_t i = 0; i < dimensions.size(); ++i) {
        full_region.end_coordinates[i] = dimensions[i].size - 1;
    }

    auto data = container->get_region_data(full_region);

    // Store modality info and delegate to variant analysis
    set_parameter("data_modality", static_cast<int>(modality));
    set_parameter("dimensions", dimensions);

    return analyze_impl(data);
}

AnalyzerOutput EnergyAnalyzer::analyze_impl(const Kakshya::Region& region)
{
    auto container_param = get_parameter("current_container");
    if (!container_param.has_value()) {
        throw std::runtime_error("Region analysis requires container context. Call analyze_impl(container) first or use analyze_impl(container, region).");
    }

    auto container = std::any_cast<std::shared_ptr<Kakshya::SignalSourceContainer>>(container_param);
    if (!container || !container->has_data()) {
        throw std::invalid_argument("Container context is invalid");
    }

    // Get the region data and analyze it
    auto data = container->get_region_data(region);
    return analyze_impl(data);
}

template <typename IntType>
std::vector<double> EnergyAnalyzer::normalize_integer_data(const std::vector<IntType>& data, double max_value)
{
    std::vector<double> result;
    result.reserve(data.size());

    const double scale = 1.0 / max_value;
    std::transform(data.begin(), data.end(), std::back_inserter(result),
        [scale](IntType val) { return static_cast<double>(val) * scale; });

    return result;
}

DataModality EnergyAnalyzer::detect_data_modality(const std::vector<Kakshya::DataDimension>& dimensions)
{
    if (dimensions.empty())
        return DataModality::UNKNOWN;

    // Count different dimension types
    size_t time_dims = 0, spatial_dims = 0, channel_dims = 0, frequency_dims = 0;

    for (const auto& dim : dimensions) {
        switch (dim.role) {
        case Kakshya::DataDimension::Role::TIME:
            time_dims++;
            break;
        case Kakshya::DataDimension::Role::SPATIAL_X:
        case Kakshya::DataDimension::Role::SPATIAL_Y:
        case Kakshya::DataDimension::Role::SPATIAL_Z:
            spatial_dims++;
            break;
        case Kakshya::DataDimension::Role::CHANNEL:
            channel_dims++;
            break;
        case Kakshya::DataDimension::Role::FREQUENCY:
            frequency_dims++;
            break;
        default:
            break;
        }
    }

    // Classify based on dimension structure - focus on audio/spectral only
    if (time_dims == 1 && spatial_dims == 0 && channel_dims <= 1) {
        return (channel_dims == 0) ? DataModality::AUDIO_1D : DataModality::AUDIO_MULTICHANNEL;
    } else if (time_dims == 1 && frequency_dims == 1) {
        return DataModality::SPECTRAL_2D;
    } else if (time_dims == 0 && spatial_dims >= 2) {
        return DataModality::IMAGE_2D; // Will be rejected
    } else if (time_dims == 1 && spatial_dims >= 2) {
        return DataModality::VIDEO_GRAYSCALE; // Will be rejected
    }

    return DataModality::TENSOR_ND;
}

// ===== Core Audio Energy Calculation Methods =====

std::vector<double> EnergyAnalyzer::calculate_energy_for_method(const std::vector<double>& data, Method method)
{
    switch (method) {
    case Method::RMS:
        return calculate_rms_energy(data);
    case Method::PEAK:
        return calculate_peak_energy(data);
    case Method::SPECTRAL:
        return calculate_spectral_energy(data);
    case Method::ZERO_CROSSING:
        return calculate_zero_crossing_energy(data);
    case Method::HARMONIC:
        return calculate_harmonic_energy(data);
    case Method::POWER:
        return calculate_power_energy(data);
    case Method::DYNAMIC_RANGE:
        return calculate_dynamic_range_energy(data);
    default:
        throw std::runtime_error("Unknown energy analysis method");
    }
}

std::vector<double> EnergyAnalyzer::calculate_rms_energy(const std::vector<double>& data)
{
    if (data.size() < m_window_size) {
        throw std::invalid_argument("Data size must be at least window size");
    }

    std::vector<double> rms_values;
    const size_t num_windows = (data.size() - m_window_size) / m_hop_size + 1;
    rms_values.reserve(num_windows);

    // Create Hanning window for windowing
    Eigen::VectorXd window = create_window_function("hanning", m_window_size);

    // Use parallel execution for performance
    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::vector<double> temp_rms(num_windows);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * m_hop_size;
            const size_t end_idx = std::min(start_idx + m_window_size, data.size());

            // Map data window to Eigen vector for efficient computation
            Eigen::Map<const Eigen::VectorXd> data_window(
                data.data() + start_idx,
                end_idx - start_idx);

            // Apply window function
            Eigen::VectorXd windowed = data_window.cwiseProduct(
                window.head(end_idx - start_idx));

            // Calculate RMS: sqrt(mean(x^2))
            double rms = std::sqrt(windowed.array().square().mean());
            temp_rms[i] = rms;
        });

    return temp_rms;
}

std::vector<double> EnergyAnalyzer::calculate_peak_energy(const std::vector<double>& data)
{
    std::vector<double> peak_values;
    const size_t num_windows = (data.size() - m_window_size) / m_hop_size + 1;
    peak_values.reserve(num_windows);

    for (size_t i = 0; i < num_windows; ++i) {
        const size_t start_idx = i * m_hop_size;
        const size_t end_idx = std::min(start_idx + m_window_size, data.size());

        // Use Eigen for efficient max computation
        Eigen::Map<const Eigen::VectorXd> data_window(
            data.data() + start_idx,
            end_idx - start_idx);

        double peak = data_window.cwiseAbs().maxCoeff();
        peak_values.push_back(peak);
    }

    return peak_values;
}

std::vector<double> EnergyAnalyzer::calculate_spectral_energy(const std::vector<double>& data)
{
    std::vector<double> spectral_energy;
    const size_t num_windows = (data.size() - m_window_size) / m_hop_size + 1;
    spectral_energy.reserve(num_windows);

    // Initialize FFT
    Eigen::FFT<double> fft;
    Eigen::VectorXd window = create_window_function("hanning", m_window_size);

    for (size_t i = 0; i < num_windows; ++i) {
        const size_t start_idx = i * m_hop_size;
        const size_t end_idx = std::min(start_idx + m_window_size, data.size());

        // Prepare windowed data
        Eigen::VectorXd windowed_data(m_window_size);
        windowed_data.setZero();

        const size_t actual_size = end_idx - start_idx;
        Eigen::Map<const Eigen::VectorXd> data_segment(
            data.data() + start_idx, actual_size);

        windowed_data.head(actual_size) = data_segment.cwiseProduct(
            window.head(actual_size));

        // Compute FFT
        Eigen::VectorXcd fft_result;
        fft.fwd(fft_result, windowed_data);

        // Calculate spectral energy (sum of magnitude squared)
        double energy = fft_result.cwiseAbs2().sum();
        spectral_energy.push_back(energy);
    }

    return spectral_energy;
}

std::vector<double> EnergyAnalyzer::calculate_zero_crossing_energy(const std::vector<double>& data)
{
    std::vector<double> zc_energy;
    const size_t num_windows = (data.size() - m_window_size) / m_hop_size + 1;
    zc_energy.reserve(num_windows);

    for (size_t i = 0; i < num_windows; ++i) {
        const size_t start_idx = i * m_hop_size;
        const size_t end_idx = std::min(start_idx + m_window_size, data.size());

        // Count zero crossings in window
        size_t zero_crossings = 0;
        for (size_t j = start_idx + 1; j < end_idx; ++j) {
            if ((data[j] >= 0.0) != (data[j - 1] >= 0.0)) {
                ++zero_crossings;
            }
        }

        // Normalize by window size to get crossing rate
        double crossing_rate = static_cast<double>(zero_crossings) / (end_idx - start_idx);
        zc_energy.push_back(crossing_rate);
    }

    return zc_energy;
}

std::vector<double> EnergyAnalyzer::calculate_harmonic_energy(const std::vector<double>& data)
{
    std::vector<double> harmonic_energy;
    const size_t num_windows = (data.size() - m_window_size) / m_hop_size + 1;
    harmonic_energy.reserve(num_windows);

    Eigen::FFT<double> fft;
    Eigen::VectorXd window = create_window_function("hanning", m_window_size);

    for (size_t i = 0; i < num_windows; ++i) {
        const size_t start_idx = i * m_hop_size;
        const size_t end_idx = std::min(start_idx + m_window_size, data.size());

        // Prepare windowed data
        Eigen::VectorXd windowed_data(m_window_size);
        windowed_data.setZero();

        const size_t actual_size = end_idx - start_idx;
        Eigen::Map<const Eigen::VectorXd> data_segment(
            data.data() + start_idx, actual_size);

        windowed_data.head(actual_size) = data_segment.cwiseProduct(
            window.head(actual_size));

        // Compute FFT
        Eigen::VectorXcd fft_result;
        fft.fwd(fft_result, windowed_data);

        // Calculate harmonic energy (focus on lower frequencies)
        const size_t harmonic_bins = fft_result.size() / 8; // Focus on lower 1/8 of spectrum
        double energy = fft_result.head(harmonic_bins).cwiseAbs2().sum();
        harmonic_energy.push_back(energy);
    }

    return harmonic_energy;
}

std::vector<double> EnergyAnalyzer::calculate_power_energy(const std::vector<double>& data)
{
    std::vector<double> power_values;
    const size_t num_windows = (data.size() - m_window_size) / m_hop_size + 1;
    power_values.reserve(num_windows);

    for (size_t i = 0; i < num_windows; ++i) {
        const size_t start_idx = i * m_hop_size;
        const size_t end_idx = std::min(start_idx + m_window_size, data.size());

        Eigen::Map<const Eigen::VectorXd> data_window(
            data.data() + start_idx,
            end_idx - start_idx);

        // Power is sum of squares
        double power = data_window.array().square().sum();
        power_values.push_back(power);
    }

    return power_values;
}

std::vector<double> EnergyAnalyzer::calculate_dynamic_range_energy(const std::vector<double>& data)
{
    std::vector<double> dynamic_range;
    const size_t num_windows = (data.size() - m_window_size) / m_hop_size + 1;
    dynamic_range.reserve(num_windows);

    for (size_t i = 0; i < num_windows; ++i) {
        const size_t start_idx = i * m_hop_size;
        const size_t end_idx = std::min(start_idx + m_window_size, data.size());

        Eigen::Map<const Eigen::VectorXd> data_window(
            data.data() + start_idx,
            end_idx - start_idx);

        double max_val = data_window.cwiseAbs().maxCoeff();
        double min_val = data_window.cwiseAbs().minCoeff();

        // Dynamic range in dB
        double range_db = (max_val > 0.0 && min_val > 0.0) ? 20.0 * std::log10(max_val / std::max(min_val, 1e-10)) : 0.0;

        dynamic_range.push_back(range_db);
    }

    return dynamic_range;
}

// ===== Utility Methods =====

Eigen::VectorXd EnergyAnalyzer::create_window_function(const std::string& window_type, size_t size)
{
    Eigen::VectorXd window(size);

    if (window_type == "hanning" || window_type == "hann") {
        for (size_t i = 0; i < size; ++i) {
            window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (size - 1)));
        }
    } else if (window_type == "hamming") {
        for (size_t i = 0; i < size; ++i) {
            window[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (size - 1));
        }
    } else if (window_type == "blackman") {
        for (size_t i = 0; i < size; ++i) {
            double factor = 2.0 * M_PI * i / (size - 1);
            window[i] = 0.42 - 0.5 * std::cos(factor) + 0.08 * std::cos(2.0 * factor);
        }
    } else if (window_type == "rectangular" || window_type == "rect") {
        window.setOnes();
    } else {
        // Default to Hanning
        for (size_t i = 0; i < size; ++i) {
            window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (size - 1)));
        }
    }

    return window;
}

AnalyzerOutput EnergyAnalyzer::format_output_based_on_granularity(
    const std::vector<double>& energy_values,
    const std::string& method)
{
    const AnalysisGranularity granularity = get_output_granularity();

    switch (granularity) {
    case AnalysisGranularity::RAW_VALUES:
        return energy_values;

    case AnalysisGranularity::ATTRIBUTED_SEGMENTS:
        return create_energy_segments(energy_values, method);

    case AnalysisGranularity::ORGANIZED_GROUPS:
        return create_energy_regions(energy_values, method);

    default:
        return energy_values;
    }
}

Kakshya::RegionGroup EnergyAnalyzer::create_energy_regions(
    const std::vector<double>& values,
    const std::string& method)
{
    Kakshya::RegionGroup group;
    group.name = "energy_analysis_" + method;

    if (!m_classification_enabled) {
        bool in_region = false;
        size_t region_start = 0;

        for (size_t i = 0; i < values.size(); ++i) {
            bool above_threshold = values[i] > m_quiet_threshold;

            if (above_threshold && !in_region) {
                // Start new region

                region_start = i * m_hop_size;
                in_region = true;
            } else if (!above_threshold && in_region) {

                // End current region
                Kakshya::Region region;
                region.start_coordinates = { region_start };
                region.end_coordinates = { i * m_hop_size };
                Kakshya::set_region_attribute(region, "energy_method", method);

                // region_start is in samples, m_hop_size is in samples per window, so convert to window index
                size_t region_start_window = region_start / m_hop_size;
                size_t region_end_window = i;
                size_t num_windows = region_end_window - region_start_window;
                double avg_energy = (num_windows > 0)
                    ? std::accumulate(values.begin() + region_start_window, values.begin() + region_end_window, 0.0) / num_windows
                    : 0.0;
                Kakshya::set_region_attribute(region, "average_energy", avg_energy);

                group.regions.push_back(std::move(region));
                in_region = false;
            }
        }

        if (in_region) {
            Kakshya::Region region;
            region.start_coordinates = { region_start };
            region.end_coordinates = { values.size() * m_hop_size };
            Kakshya::set_region_attribute(region, "energy_method", method);
            group.regions.push_back(std::move(region));
        }
    } else {
        for (size_t i = 0; i < values.size(); ++i) {
            EnergyLevel level = classify_energy_level(values[i]);

            Kakshya::Region region;
            region.start_coordinates = { i * m_hop_size };
            region.end_coordinates = { (i + 1) * m_hop_size };

            Kakshya::set_region_attribute(region, "energy_method", method);
            Kakshya::set_region_attribute(region, "energy_value", values[i]);
            Kakshya::set_region_attribute(region, "energy_level", energy_level_to_string(level));

            group.regions.push_back(std::move(region));
        }
    }

    return group;
}

std::vector<Kakshya::RegionSegment> EnergyAnalyzer::create_energy_segments(
    const std::vector<double>& values,
    const std::string& method)
{
    std::vector<Kakshya::RegionSegment> segments;
    segments.reserve(values.size());

    for (size_t i = 0; i < values.size(); ++i) {
        Kakshya::RegionSegment segment;
        segment.source_region.start_coordinates = { i * m_hop_size };
        segment.source_region.end_coordinates = { (i + 1) * m_hop_size };

        Kakshya::set_region_attribute(segment.source_region, "energy_method", method);
        Kakshya::set_region_attribute(segment.source_region, "energy_value", values[i]);

        if (m_classification_enabled) {
            EnergyLevel level = classify_energy_level(values[i]);
            Kakshya::set_region_attribute(segment.source_region, "energy_level",
                energy_level_to_string(level));
        }

        segments.push_back(std::move(segment));
    }

    return segments;
}

void EnergyAnalyzer::set_energy_thresholds(double silent, double quiet, double moderate, double loud)
{
    if (silent >= quiet || quiet >= moderate || moderate >= loud) {
        throw std::invalid_argument("Thresholds must be in ascending order");
    }

    m_silent_threshold = silent;
    m_quiet_threshold = quiet;
    m_moderate_threshold = moderate;
    m_loud_threshold = loud;
}

void EnergyAnalyzer::set_window_parameters(uint32_t window_size, uint32_t hop_size)
{
    if (window_size == 0 || hop_size == 0) {
        throw std::invalid_argument("Window and hop sizes must be greater than 0");
    }

    m_window_size = window_size;
    m_hop_size = hop_size;
}

EnergyAnalyzer::EnergyLevel EnergyAnalyzer::classify_energy_level(double energy) const
{
    if (energy <= m_silent_threshold)
        return EnergyLevel::SILENT;
    if (energy <= m_quiet_threshold)
        return EnergyLevel::QUIET;
    if (energy <= m_moderate_threshold)
        return EnergyLevel::MODERATE;
    if (energy <= m_loud_threshold)
        return EnergyLevel::LOUD;
    return EnergyLevel::PEAK;
}

std::string EnergyAnalyzer::energy_level_to_string(EnergyLevel level) const
{
    switch (level) {
    case EnergyLevel::SILENT:
        return "silent";
    case EnergyLevel::QUIET:
        return "quiet";
    case EnergyLevel::MODERATE:
        return "moderate";
    case EnergyLevel::LOUD:
        return "loud";
    case EnergyLevel::PEAK:
        return "peak";
    default:
        return "unknown";
    }
}

std::string EnergyAnalyzer::method_to_string(Method method)
{
    switch (method) {
    case Method::RMS:
        return "rms";
    case Method::PEAK:
        return "peak";
    case Method::SPECTRAL:
        return "spectral";
    case Method::ZERO_CROSSING:
        return "zero_crossing";
    case Method::HARMONIC:
        return "harmonic";
    case Method::POWER:
        return "power";
    case Method::DYNAMIC_RANGE:
        return "dynamic_range";
    default:
        return "unknown";
    }
}

EnergyAnalyzer::Method EnergyAnalyzer::string_to_method(const std::string& str)
{
    if (str == "rms" || str == "default")
        return Method::RMS;
    if (str == "peak")
        return Method::PEAK;
    if (str == "spectral")
        return Method::SPECTRAL;
    if (str == "zero_crossing")
        return Method::ZERO_CROSSING;
    if (str == "harmonic")
        return Method::HARMONIC;
    if (str == "power")
        return Method::POWER;
    if (str == "dynamic_range")
        return Method::DYNAMIC_RANGE;

    throw std::invalid_argument("Unknown energy analysis method: " + str);
}

// ===== Factory Functions =====
// TODO: Need to rewrite parent to allow consturcor params

/* void register_analyzer_operations(std::shared_ptr<ComputeMatrix> matrix)
{
    if (!matrix) {
        throw std::invalid_argument("ComputeMatrix cannot be null");
    }

    matrix->register_operation<UniversalAnalyzer>("universal_analyzer");

    matrix->register_operation<EnergyAnalyzer>("energy_analyzer");

    matrix->register_operation<DataToValues>("data_to_energy_values");
    matrix->register_operation<ContainerToRegions>("container_to_energy_regions");
    matrix->register_operation<RegionToSegments>("region_to_energy_segments");
} */

}
