#include "Yantra.hpp"

#include "MayaFlux/Nodes/Generators/WindowGenerator.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"

namespace MayaFlux {

namespace {
    std::vector<double> extract_double_vector(const Kakshya::DataVariant& data)
    {
        return Yantra::OperationHelper::extract_as_double(data);
    }
}

//=========================================================================
// STATISTICAL ANALYSIS - Use StatisticalAnalyzer
//=========================================================================

double mean(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StatisticalAnalyzer<>>();
    analyzer->set_method(Yantra::StatisticalMethod::MEAN);
    auto result = analyzer->analyze_statistics(Kakshya::DataVariant(data));
    return result.mean_stat;
}

double mean(const Kakshya::DataVariant& data)
{
    return mean(extract_double_vector(data));
}

double rms(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StatisticalAnalyzer<>>();
    analyzer->set_method(Yantra::StatisticalMethod::RMS);
    auto result = analyzer->analyze_statistics(Kakshya::DataVariant(data));
    return result.statistical_values.empty() ? 0.0 : result.statistical_values[0];
}

double rms(const Kakshya::DataVariant& data)
{
    return rms(extract_double_vector(data));
}

double std_dev(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StatisticalAnalyzer<>>();
    analyzer->set_method(Yantra::StatisticalMethod::STD_DEV);
    auto result = analyzer->analyze_statistics(Kakshya::DataVariant(data));
    return result.stat_std_dev;
}

double std_dev(const Kakshya::DataVariant& data)
{
    return std_dev(extract_double_vector(data));
}

double dynamic_range(const std::vector<double>& data)
{
    auto energy_analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::DYNAMIC_RANGE);
    auto result = energy_analyzer->analyze_energy(Kakshya::DataVariant(data));
    return result.energy_values.empty() ? 0.0 : result.energy_values[0];
}

double dynamic_range(const Kakshya::DataVariant& data)
{
    return dynamic_range(extract_double_vector(data));
}

double peak(const std::vector<double>& data)
{
    auto energy_analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::PEAK);
    auto result = energy_analyzer->analyze_energy(Kakshya::DataVariant(data));
    return result.max_energy;
}

double peak(const Kakshya::DataVariant& data)
{
    return peak(extract_double_vector(data));
}

//=========================================================================
// FEATURE EXTRACTION - Use FeatureExtractor to get ACTUAL data
//=========================================================================

std::vector<size_t> zero_crossings(const std::vector<double>& data, double threshold)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::ZERO_CROSSING_DATA);
    extractor->set_parameter("threshold", threshold);
    extractor->set_parameter("min_distance", 1.0);
    extractor->set_parameter("region_size", 1U);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = extractor->apply_operation(input);

    std::vector<size_t> crossings;
    auto window_positions_opt = result.template get_metadata<std::vector<std::pair<size_t, size_t>>>("window_positions");
    if (window_positions_opt.has_value()) {
        for (const auto& [start, end] : window_positions_opt.value()) {
            crossings.push_back(start);
        }
    }
    return crossings;
}

std::vector<size_t> zero_crossings(const Kakshya::DataVariant& data, double threshold)
{
    return zero_crossings(extract_double_vector(data), threshold);
}

double zero_crossing_rate(const std::vector<double>& data, size_t window_size)
{
    auto analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    analyzer->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    if (window_size > 0) {
        analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = analyzer->analyze_energy(Kakshya::DataVariant(data));
    return result.mean_energy;
}

double zero_crossing_rate(const Kakshya::DataVariant& data, size_t window_size)
{
    return zero_crossing_rate(extract_double_vector(data), window_size);
}

double spectral_centroid(const std::vector<double>& data, double sample_rate)
{
    auto energy_analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    energy_analyzer->set_parameter("sample_rate", sample_rate);
    auto result = energy_analyzer->analyze_energy(Kakshya::DataVariant(data));
    return result.mean_energy;
}

double spectral_centroid(const Kakshya::DataVariant& data, double sample_rate)
{
    return spectral_centroid(extract_double_vector(data), sample_rate);
}

std::vector<double> detect_onsets(const std::vector<double>& data, double sample_rate, double threshold)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>(1024, 512, Yantra::ExtractionMethod::HIGH_ENERGY_DATA);
    extractor->set_parameter("energy_threshold", threshold);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = extractor->apply_operation(input);

    std::vector<double> onsets;
    auto positions_opt = result.template get_metadata<std::vector<std::pair<size_t, size_t>>>("window_positions");
    if (positions_opt.has_value()) {
        for (const auto& [start, end] : positions_opt.value()) {
            double onset_time = double(start) / sample_rate;
            onsets.push_back(onset_time);
        }
    }

    return onsets;
}

std::vector<double> detect_onsets(const Kakshya::DataVariant& data, double sample_rate, double threshold)
{
    return detect_onsets(extract_double_vector(data), sample_rate, threshold);
}

//=========================================================================
// BASIC TRANSFORMATIONS - Use MathematicalTransformer
//=========================================================================

void apply_gain(std::vector<double>& data, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = transformer->apply_operation(input);
    data = extract_double_vector(result.data);
}

void apply_gain(Kakshya::DataVariant& data, double gain_factor)
{
    auto double_data = extract_double_vector(data);
    apply_gain(double_data, gain_factor);
    data = Kakshya::DataVariant(double_data);
}

std::vector<double> with_gain(const std::vector<double>& data, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = transformer->apply_operation(input);
    return extract_double_vector(result.data);
}

Kakshya::DataVariant with_gain(const Kakshya::DataVariant& data, double gain_factor)
{
    return { with_gain(extract_double_vector(data), gain_factor) };
}

void normalize(std::vector<double>& data, double target_peak)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", target_peak);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = transformer->apply_operation(input);
    data = extract_double_vector(result.data);
}

void normalize(Kakshya::DataVariant& data, double target_peak)
{
    auto double_data = extract_double_vector(data);
    normalize(double_data, target_peak);
    data = Kakshya::DataVariant(double_data);
}

std::vector<double> normalized(const std::vector<double>& data, double target_peak)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", target_peak);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = transformer->apply_operation(input);
    return extract_double_vector(result.data);
}

Kakshya::DataVariant normalized(const Kakshya::DataVariant& data, double target_peak)
{
    return { normalized(extract_double_vector(data), target_peak) };
}

//=========================================================================
// TEMPORAL TRANSFORMATIONS - Use TemporalTransformer
//=========================================================================

void reverse(std::vector<double>& data)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = transformer->apply_operation(input);
    data = extract_double_vector(result.data);
}

void reverse(Kakshya::DataVariant& data)
{
    auto double_data = extract_double_vector(data);
    reverse(double_data);
    data = Kakshya::DataVariant(double_data);
}

std::vector<double> reversed(const std::vector<double>& data)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = transformer->apply_operation(input);
    return extract_double_vector(result.data);
}

Kakshya::DataVariant reversed(const Kakshya::DataVariant& data)
{
    return { reversed(extract_double_vector(data)) };
}

//=========================================================================
// FREQUENCY DOMAIN - Use SpectralTransformer and EnergyAnalyzer
//=========================================================================

std::vector<double> magnitude_spectrum(const std::vector<double>& data, size_t window_size)
{
    auto energy_analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    if (window_size > 0) {
        energy_analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = energy_analyzer->analyze_energy(Kakshya::DataVariant(data));
    return result.energy_values;
}

std::vector<double> magnitude_spectrum(const Kakshya::DataVariant& data, size_t window_size)
{
    return magnitude_spectrum(extract_double_vector(data), window_size);
}

std::vector<double> power_spectrum(const std::vector<double>& data, size_t window_size)
{
    auto energy_analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::POWER);
    if (window_size > 0) {
        energy_analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = energy_analyzer->analyze_energy(Kakshya::DataVariant(data));
    return result.energy_values;
}

std::vector<double> power_spectrum(const Kakshya::DataVariant& data, size_t window_size)
{
    return power_spectrum(extract_double_vector(data), window_size);
}

//=========================================================================
// PITCH AND TIME - Use EnergyAnalyzer for harmonic analysis
//=========================================================================

double estimate_pitch(const std::vector<double>& data, double sample_rate,
    double min_freq, double max_freq)
{
    auto energy_analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::HARMONIC);
    energy_analyzer->set_parameter("sample_rate", sample_rate);
    energy_analyzer->set_parameter("min_freq", min_freq);
    energy_analyzer->set_parameter("max_freq", max_freq);
    auto result = energy_analyzer->analyze_energy(Kakshya::DataVariant(data));

    return result.mean_energy * sample_rate / 1000.0;
}

double estimate_pitch(const Kakshya::DataVariant& data, double sample_rate,
    double min_freq, double max_freq)
{
    return estimate_pitch(extract_double_vector(data), sample_rate, min_freq, max_freq);
}

//=========================================================================
// WINDOWING AND SEGMENTATION - Use FeatureExtractor and MathematicalTransformer
//=========================================================================

std::vector<double> extract_silent_data(const std::vector<double>& data,
    double threshold,
    size_t min_silence_duration)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
    extractor->set_parameter("silence_threshold", threshold);
    extractor->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = extractor->apply_operation(input);

    return extract_double_vector(result.data);
}

std::vector<double> extract_silent_data(const Kakshya::DataVariant& data,
    double threshold,
    size_t min_silence_duration)
{
    return extract_silent_data(extract_double_vector(data), threshold, min_silence_duration);
}

std::vector<double> extract_zero_crossing_regions(const std::vector<double>& data,
    double threshold,
    size_t region_size)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::ZERO_CROSSING_DATA);
    extractor->set_parameter("threshold", threshold);
    extractor->set_parameter("min_distance", 1.0);
    extractor->set_parameter("region_size", static_cast<uint32_t>(region_size));

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = extractor->apply_operation(input);

    return extract_double_vector(result.data);
}

std::vector<double> extract_zero_crossing_regions(const Kakshya::DataVariant& data,
    double threshold,
    size_t region_size)
{
    return extract_zero_crossing_regions(extract_double_vector(data), threshold, region_size);
}

void apply_window(std::vector<double>& data, const std::string& window_type)
{
    Nodes::Generator::WindowType win_type;

    if (window_type == "hann" || window_type == "hanning") {
        win_type = Nodes::Generator::WindowType::HANNING;
    } else if (window_type == "hamming") {
        win_type = Nodes::Generator::WindowType::HAMMING;
    } else if (window_type == "blackman") {
        win_type = Nodes::Generator::WindowType::BLACKMAN;
    } else if (window_type == "rectangular" || window_type == "rect") {
        win_type = Nodes::Generator::WindowType::RECTANGULAR;
    } else {
        win_type = Nodes::Generator::WindowType::HANNING;
    }

    auto window = Nodes::Generator::generate_window(data.size(), win_type);

    for (size_t i = 0; i < data.size() && i < window.size(); ++i) {
        data[i] *= window[i];
    }
}

void apply_window(Kakshya::DataVariant& data, const std::string& window_type)
{
    auto double_data = extract_double_vector(data);
    apply_window(double_data, window_type);
    data = Kakshya::DataVariant(double_data);
}

std::vector<std::vector<double>> windowed_segments(const std::vector<double>& data,
    size_t window_size,
    size_t hop_size)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>(window_size, hop_size,
        Yantra::ExtractionMethod::OVERLAPPING_WINDOWS);
    extractor->set_parameter("overlap", double(hop_size) / window_size);

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = extractor->apply_operation(input);

    auto extracted_data = extract_double_vector(result.data);

    std::vector<std::vector<double>> segments;
    for (size_t i = 0; i < extracted_data.size(); i += window_size) {
        size_t end_idx = std::min(i + window_size, extracted_data.size());
        segments.emplace_back(extracted_data.begin() + i, extracted_data.begin() + end_idx);
    }

    return segments;
}

std::vector<std::vector<double>> windowed_segments(const Kakshya::DataVariant& data,
    size_t window_size,
    size_t hop_size)
{
    return windowed_segments(extract_double_vector(data), window_size, hop_size);
}

std::vector<std::pair<size_t, size_t>> detect_silence(const std::vector<double>& data,
    double threshold,
    size_t min_silence_duration)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
    extractor->set_parameter("silence_threshold", threshold);
    extractor->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));

    Yantra::IO<Kakshya::DataVariant> input { Kakshya::DataVariant(data) };
    auto result = extractor->apply_operation(input);

    std::vector<std::pair<size_t, size_t>> silence_regions;
    auto window_positions_opt = result.template get_metadata<std::vector<std::pair<size_t, size_t>>>("window_positions");
    if (window_positions_opt.has_value()) {
        silence_regions = window_positions_opt.value();
    }

    return silence_regions;
}

std::vector<std::pair<size_t, size_t>> detect_silence(const Kakshya::DataVariant& data,
    double threshold,
    size_t min_silence_duration)
{
    return detect_silence(extract_double_vector(data), threshold, min_silence_duration);
}

//=========================================================================
// UTILITY AND CONVERSION - Use MathematicalTransformer for mixing
//=========================================================================

std::vector<double> mix(const std::vector<std::vector<double>>& streams)
{
    if (streams.empty())
        return {};

    std::vector<double> result(streams[0].size(), 0.0);
    for (const auto& stream : streams) {
        for (size_t i = 0; i < std::min(result.size(), stream.size()); ++i) {
            result[i] += stream[i];
        }
    }

    if (!streams.empty()) {
        double gain = 1.0 / streams.size();
        apply_gain(result, gain);
    }

    return result;
}

std::vector<double> mix(const std::vector<Kakshya::DataVariant>& streams)
{
    std::vector<std::vector<double>> double_streams;
    for (const auto& stream : streams) {
        double_streams.push_back(extract_double_vector(stream));
    }
    return mix(double_streams);
}

std::vector<double> mix_with_gains(const std::vector<std::vector<double>>& streams,
    const std::vector<double>& gains)
{
    if (streams.empty() || gains.size() != streams.size())
        return {};

    std::vector<double> result(streams[0].size(), 0.0);
    for (size_t s = 0; s < streams.size(); ++s) {
        auto gained_stream = with_gain(streams[s], gains[s]);
        for (size_t i = 0; i < std::min(result.size(), gained_stream.size()); ++i) {
            result[i] += gained_stream[i];
        }
    }

    return result;
}

std::vector<double> mix_with_gains(const std::vector<Kakshya::DataVariant>& streams,
    const std::vector<double>& gains)
{
    std::vector<std::vector<double>> double_streams;
    for (const auto& stream : streams) {
        double_streams.push_back(extract_double_vector(stream));
    }
    return mix_with_gains(double_streams, gains);
}

std::vector<double> to_double_vector(const Kakshya::DataVariant& data)
{
    return extract_double_vector(data);
}

Kakshya::DataVariant to_data_variant(const std::vector<double>& data)
{
    return { data };
}

//=========================================================================
// INITIALIZATION
//=========================================================================

void initialize_yantra()
{
    // Initialize any global Yantra state if needed
    // Operations are generally stateless, so minimal initialization required
}

} // namespace MayaFlux
