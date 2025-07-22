#include "EnergyAnalyzer.hpp"
#include "AnalysisHelpers.hpp"

#include "MayaFlux/Kakshya/KakshyaUtils.hpp"
#include "MayaFlux/Nodes/Generators/Generator.hpp"

#include "MayaFlux/EnumUtils.hpp"

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

    UniversalAnalyzer::set_parameter("window_function", std::string("hanning"));
    UniversalAnalyzer::set_parameter("overlap_add", true);
    UniversalAnalyzer::set_parameter("zero_padding", false);
}

std::vector<std::string> EnergyAnalyzer::get_available_methods() const
{
    auto names = Utils::get_enum_names_lowercase<Method>();
    return std::vector<std::string>(names.begin(), names.end());
}

std::vector<std::string> EnergyAnalyzer::get_methods_for_type_impl(std::type_index type_info) const
{
    return get_available_methods();
}

AnalyzerOutput EnergyAnalyzer::analyze_impl(const Kakshya::DataVariant& data)
{
    const std::string method = get_analysis_method();
    const Method energy_method = string_to_method(method);

    std::vector<double> audio_data = Kakshya::convert_variant_to_double(data);

    std::vector<double> energy_values = calculate_energy_for_method(audio_data, energy_method);

    return format_analyzer_output(
        *this, energy_values, method,
        [this](const auto& values, const auto& method_name) {
            return create_energy_regions(values, method_name);
        },
        [this](const auto& values, const auto& method_name) {
            return create_energy_segments(values, method_name);
        });
}

AnalyzerOutput EnergyAnalyzer::analyze_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    auto [validated_container, dimensions] = Kakshya::validate_container_for_analysis(container);

    Kakshya::DataModality modality = Kakshya::detect_data_modality(dimensions);

    if (modality != Kakshya::DataModality::AUDIO_1D && modality != Kakshya::DataModality::AUDIO_MULTICHANNEL && modality != Kakshya::DataModality::SPECTRAL_2D) {
        throw std::runtime_error("EnergyAnalyzer only handles audio and spectral data as the concepts of RMS/PEAK etc.. dont yield themselves to multimodal data.\n Use specialized analyzers for images/video.");
    }

    Kakshya::Region full_region;
    full_region.start_coordinates.resize(dimensions.size(), 0);
    full_region.end_coordinates.resize(dimensions.size());

    for (size_t i = 0; i < dimensions.size(); ++i) {
        full_region.end_coordinates[i] = dimensions[i].size - 1;
    }

    auto data = validated_container->get_region_data(full_region);

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

    Kakshya::validate_container_for_analysis(container);

    auto data = container->get_region_data(region);
    return analyze_impl(data);
}

AnalyzerOutput EnergyAnalyzer::analyze_impl(const Kakshya::RegionGroup& group)
{
    std::vector<double> results;

    for (const auto& region : group.regions) {
        try {
            auto region_result = analyze_impl(region);
            if (std::holds_alternative<std::vector<double>>(region_result)) {
                const auto& values = std::get<std::vector<double>>(region_result);
                if (!values.empty()) {
                    double region_energy = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
                    results.push_back(region_energy);
                }
            }
        } catch (const std::exception& e) {
            continue;
        }
    }

    if (results.empty()) {
        throw std::runtime_error("No energy data could be extracted from regions in group");
    }

    const std::string method = get_analysis_method();
    return format_output_based_on_granularity(results, method);
}

AnalyzerOutput EnergyAnalyzer::analyze_impl(const std::vector<Kakshya::RegionSegment>& segments)
{
    std::vector<double> results;

    for (const auto& segment : segments) {
        try {
            auto segment_result = analyze_impl(segment.source_region);
            if (std::holds_alternative<std::vector<double>>(segment_result)) {
                const auto& values = std::get<std::vector<double>>(segment_result);
                if (!values.empty()) {
                    double segment_energy = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
                    results.push_back(segment_energy);
                }
            }
        } catch (const std::exception& e) {
            continue;
        }
    }

    if (results.empty()) {
        throw std::runtime_error("No energy data could be extracted from segments");
    }

    const std::string method = get_analysis_method();
    return format_output_based_on_granularity(results, method);
}

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

    Eigen::VectorXd window = create_window_function("hanning", m_window_size);

    std::vector<size_t> indices(num_windows);
    std::iota(indices.begin(), indices.end(), 0);

    std::vector<double> temp_rms(num_windows);

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](size_t i) {
            const size_t start_idx = i * m_hop_size;
            const size_t end_idx = std::min(start_idx + m_window_size, data.size());

            Eigen::Map<const Eigen::VectorXd> data_window(
                data.data() + start_idx,
                end_idx - start_idx);

            Eigen::VectorXd windowed = data_window.cwiseProduct(
                window.head(end_idx - start_idx));

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

    Eigen::FFT<double> fft;
    Eigen::VectorXd window = create_window_function("hanning", m_window_size);

    for (size_t i = 0; i < num_windows; ++i) {
        const size_t start_idx = i * m_hop_size;
        const size_t end_idx = std::min(start_idx + m_window_size, data.size());

        Eigen::VectorXd windowed_data(m_window_size);
        windowed_data.setZero();

        const size_t actual_size = end_idx - start_idx;
        Eigen::Map<const Eigen::VectorXd> data_segment(
            data.data() + start_idx, actual_size);

        windowed_data.head(actual_size) = data_segment.cwiseProduct(
            window.head(actual_size));

        Eigen::VectorXcd fft_result;
        fft.fwd(fft_result, windowed_data);

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

        size_t zero_crossings = 0;
        for (size_t j = start_idx + 1; j < end_idx; ++j) {
            if ((data[j] >= 0.0) != (data[j - 1] >= 0.0)) {
                ++zero_crossings;
            }
        }

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

        Eigen::VectorXd windowed_data(m_window_size);
        windowed_data.setZero();

        const size_t actual_size = end_idx - start_idx;
        Eigen::Map<const Eigen::VectorXd> data_segment(
            data.data() + start_idx, actual_size);

        windowed_data.head(actual_size) = data_segment.cwiseProduct(
            window.head(actual_size));

        Eigen::VectorXcd fft_result;
        fft.fwd(fft_result, windowed_data);

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

        double range_db = (max_val > 0.0 && min_val > 0.0) ? 20.0 * std::log10(max_val / std::max(min_val, 1e-10)) : 0.0;

        dynamic_range.push_back(range_db);
    }

    return dynamic_range;
}

Eigen::VectorXd EnergyAnalyzer::create_window_function(Utils::WindowType window_type, size_t size)
{
    std::vector<double> window_vector;
    if (window_type == Utils::WindowType::HANNING) {
        window_vector = MayaFlux::Nodes::Generator::HannWindow(size);
    } else if (window_type == Utils::WindowType::HAMMING) {
        window_vector = MayaFlux::Nodes::Generator::HammingWindow(size);
    } else if (window_type == Utils::WindowType::BLACKMAN) {
        window_vector = MayaFlux::Nodes::Generator::BlackmanWindow(size);
    } else if (window_type == Utils::WindowType::RECTANGULAR) {
        window_vector.resize(size, 1.0);
    } else {
        window_vector = MayaFlux::Nodes::Generator::HannWindow(size);
    }

    return Eigen::Map<Eigen::VectorXd>(window_vector.data(), window_vector.size());
}

Eigen::VectorXd EnergyAnalyzer::create_window_function(const std::string& window_type, size_t size)
{
    std::vector<double> window_vector;

    if (window_type == "hanning" || window_type == "hann") {
        window_vector = MayaFlux::Nodes::Generator::HannWindow(size);
    } else if (window_type == "hamming") {
        window_vector = MayaFlux::Nodes::Generator::HammingWindow(size);
    } else if (window_type == "blackman") {
        window_vector = MayaFlux::Nodes::Generator::BlackmanWindow(size);
    } else if (window_type == "rectangular" || window_type == "rect") {
        window_vector.resize(size, 1.0);
    } else {
        window_vector = MayaFlux::Nodes::Generator::HannWindow(size);
    }

    return Eigen::Map<Eigen::VectorXd>(window_vector.data(), window_vector.size());
}

AnalyzerOutput EnergyAnalyzer::format_output_based_on_granularity(const std::vector<double>& energy_values, const std::string& method)
{
    return format_analyzer_output(*this, energy_values, method, [this](const auto& values, const auto& method_name) { return create_energy_regions(values, method_name); }, [this](const auto& values, const auto& method_name) { return create_energy_segments(values, method_name); });
}

Kakshya::RegionGroup EnergyAnalyzer::create_energy_regions(const std::vector<double>& values, const std::string& method)
{
    Kakshya::RegionGroup group;
    group.name = std::string("Energy Analysis - " + method);
    group.attributes["description"] = std::string("Energy regions based on " + method + " analysis");

    if (!values.empty()) {
        double silent_threshold = get_parameter_or_default<double>("silent_threshold", 0.001);
        double quiet_threshold = get_parameter_or_default<double>("quiet_threshold", 0.01);
        double moderate_threshold = get_parameter_or_default<double>("moderate_threshold", 0.1);
        double loud_threshold = get_parameter_or_default<double>("loud_threshold", 0.5);

        for (size_t i = 0; i < values.size(); ++i) {
            Kakshya::Region region;
            region.start_coordinates = { static_cast<u_int64_t>(i * m_hop_size) };
            region.end_coordinates = { static_cast<u_int64_t>((i + 1) * m_hop_size) };

            region.attributes["energy_value"] = values[i];
            region.attributes["frame_index"] = static_cast<int>(i);

            std::string classification;
            if (values[i] < silent_threshold) {
                classification = "silent";
            } else if (values[i] < quiet_threshold) {
                classification = "quiet";
            } else if (values[i] < moderate_threshold) {
                classification = "moderate";
            } else if (values[i] < loud_threshold) {
                classification = "loud";
            } else {
                classification = "very_loud";
            }

            region.attributes["energy_level"] = classification;
            group.regions.push_back(region);
        }
    }

    return group;
}

std::vector<Kakshya::RegionSegment> EnergyAnalyzer::create_energy_segments(const std::vector<double>& values, const std::string& method)
{
    std::vector<Kakshya::RegionSegment> segments;

    for (size_t i = 0; i < values.size(); ++i) {
        Kakshya::RegionSegment segment;

        segment.source_region.start_coordinates = { static_cast<u_int64_t>(i * m_hop_size) };
        segment.source_region.end_coordinates = { static_cast<u_int64_t>((i + 1) * m_hop_size) };

        segment.source_region.attributes["method"] = method;
        segment.source_region.attributes["energy_value"] = values[i];
        segment.source_region.attributes["frame_index"] = static_cast<int>(i);

        segments.push_back(segment);
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

std::string EnergyAnalyzer::energy_level_to_string(EnergyLevel level)
{
    return static_cast<std::string>(Utils::enum_to_lowercase_string(level));
}

std::string EnergyAnalyzer::method_to_string(Method method)
{
    return static_cast<std::string>(Utils::enum_to_lowercase_string(method));
}

EnergyAnalyzer::Method EnergyAnalyzer::string_to_method(const std::string& str)
{
    if (str == "default") {
        return Method::RMS;
    }
    return Utils::string_to_enum_or_throw_case_insensitive<Method>(str, "EnergyAnalyzer method");
}

EnergyAnalyzer::EnergyLevel EnergyAnalyzer::string_to_energy_level(const std::string& str)
{
    return Utils::string_to_enum_or_throw_case_insensitive<EnergyLevel>(str, "EnergyLevel");
}

}
