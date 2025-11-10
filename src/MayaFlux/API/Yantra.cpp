#include "Yantra.hpp"

#include "MayaFlux/Nodes/Generators/WindowGenerator.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"

namespace MayaFlux {

static bool is_same_size(const std::vector<std::span<double>>& data)
{
    if (data.empty())
        return true;

    const auto expected_size = data.front().size();
    return std::ranges::all_of(data,
        [&expected_size](const auto& v) { return v.size() == expected_size; });
}

static std::vector<double> concat_vectors(const std::vector<std::span<double>>& data)
{
    std::vector<double> result;
    result.reserve(std::accumulate(
        data.begin(), data.end(), size_t(0),
        [](size_t sum, const auto& span) { return sum + span.size(); }));

    for (const auto& span : data) {
        result.insert(result.end(), span.begin(), span.end());
    }
    return result;
}

//=========================================================================
// STATISTICAL ANALYSIS - Use StatisticalAnalyzer
//=========================================================================

double mean(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StatisticalAnalyzer<>>();
    analyzer->set_method(Yantra::StatisticalMethod::MEAN);
    auto result = analyzer->analyze_statistics({ Kakshya::DataVariant(data) });
    return result.channel_statistics[0].mean_stat;
}

double mean(const Kakshya::DataVariant& data)
{
    auto analyzer = std::make_shared<Yantra::StatisticalAnalyzer<>>();
    analyzer->set_method(Yantra::StatisticalMethod::MEAN);
    auto result = analyzer->analyze_statistics({ data });
    return result.channel_statistics[0].mean_stat;
}

std::vector<double> mean_per_channel(const std::vector<Kakshya::DataVariant>& data)
{
    auto analyzer = std::make_shared<Yantra::StatisticalAnalyzer<>>();
    analyzer->set_method(Yantra::StatisticalMethod::MEAN);
    auto result = analyzer->analyze_statistics({ data });

    std::vector<double> means;
    means.reserve(result.channel_statistics.size());

    for (const auto& stats : result.channel_statistics) {
        means.push_back(stats.mean_stat);
    }
    return means;
}

double mean_combined(const std::vector<Kakshya::DataVariant>& channels)
{
    auto data = Yantra::OperationHelper::extract_numeric_data(channels);
    if (is_same_size(data)) {
        auto result = mean_per_channel(channels);
        return std::accumulate(result.begin(), result.end(), 0.0) / result.size();
    } else {
        auto mixed = concat_vectors(data);
        return mean(mixed);
    }
}

double rms(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
    analyzer->set_method(Yantra::StatisticalMethod::RMS);
    auto result = analyzer->analyze_statistics({ Kakshya::DataVariant(data) });
    return result.channel_statistics[0].statistical_values.empty() ? 0.0 : result.channel_statistics[0].statistical_values[0];
}

double rms(const Kakshya::DataVariant& data)
{
    auto analyzer = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
    analyzer->set_method(Yantra::StatisticalMethod::RMS);
    auto result = analyzer->analyze_statistics({ data });
    return result.channel_statistics[0].statistical_values.empty() ? 0.0 : result.channel_statistics[0].statistical_values[0];
}

std::vector<double> rms_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    auto analyzer = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
    analyzer->set_method(Yantra::StatisticalMethod::RMS);
    auto result = analyzer->analyze_statistics(channels);
    std::vector<double> rms_values;
    for (const auto& stats : result.channel_statistics) {
        rms_values.push_back(stats.statistical_values.empty() ? 0.0 : stats.statistical_values[0]);
    }
    return rms_values;
}

double rms_combined(const std::vector<Kakshya::DataVariant>& channels)
{
    auto data = Yantra::OperationHelper::extract_numeric_data(channels);
    if (is_same_size(data)) {
        auto result = rms_per_channel(channels);
        double sum_squares = 0.0;
        for (double rms_val : result) {
            sum_squares += rms_val * rms_val;
        }
        return std::sqrt(sum_squares / result.size());
    } else {
        auto mixed = concat_vectors(data);
        return rms(mixed);
    }
}

double std_dev(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
    analyzer->set_method(Yantra::StatisticalMethod::STD_DEV);
    auto result = analyzer->analyze_statistics({ Kakshya::DataVariant(data) });
    return result.channel_statistics[0].stat_std_dev;
}

double std_dev(const Kakshya::DataVariant& data)
{
    auto analyzer = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
    analyzer->set_method(Yantra::StatisticalMethod::STD_DEV);
    auto result = analyzer->analyze_statistics({ data });
    return result.channel_statistics[0].stat_std_dev;
}

std::vector<double> std_dev_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    auto analyzer = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
    analyzer->set_method(Yantra::StatisticalMethod::STD_DEV);
    auto result = analyzer->analyze_statistics(channels);
    std::vector<double> std_devs;
    for (const auto& stats : result.channel_statistics) {
        std_devs.push_back(stats.stat_std_dev);
    }
    return std_devs;
}

double std_dev_combined(const std::vector<Kakshya::DataVariant>& channels)
{
    auto data = Yantra::OperationHelper::extract_numeric_data(channels);
    if (is_same_size(data)) {
        auto result = std_dev_per_channel(channels);
        return std::accumulate(result.begin(), result.end(), 0.0) / result.size();
    } else {
        auto mixed = concat_vectors(data);
        return std_dev(mixed);
    }
}

double dynamic_range(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::DYNAMIC_RANGE);
    auto result = analyzer->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() || result.channels[0].energy_values.empty() ? 0.0 : result.channels[0].energy_values[0];
}

double dynamic_range(const Kakshya::DataVariant& data)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::DYNAMIC_RANGE);
    auto result = analyzer->analyze_energy({ data });
    return result.channels.empty() || result.channels[0].energy_values.empty() ? 0.0 : result.channels[0].energy_values[0];
}

std::vector<double> dynamic_range_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::DYNAMIC_RANGE);
    auto result = analyzer->analyze_energy(channels);
    std::vector<double> ranges;
    for (const auto& channel : result.channels) {
        ranges.push_back(channel.energy_values.empty() ? 0.0 : channel.energy_values[0]);
    }
    return ranges;
}

double dynamic_range_global(const std::vector<Kakshya::DataVariant>& channels)
{
    auto data = Yantra::OperationHelper::extract_numeric_data(channels);
    double global_min = std::numeric_limits<double>::max();
    double global_max = std::numeric_limits<double>::lowest();

    for (const auto& span : data) {
        auto [min_it, max_it] = std::ranges::minmax_element(span);
        if (min_it != span.end()) {
            global_min = std::min(global_min, *min_it);
            global_max = std::max(global_max, *max_it);
        }
    }

    if (global_min <= 0.0 || global_max <= 0.0) {
        return 0.0;
    }
    return 20.0 * std::log10(global_max / std::abs(global_min));
}

double peak(const std::vector<double>& data)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::PEAK);
    auto result = analyzer->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() ? 0.0 : result.channels[0].max_energy;
}

double peak(const Kakshya::DataVariant& data)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::PEAK);
    auto result = analyzer->analyze_energy({ data });
    return result.channels.empty() ? 0.0 : result.channels[0].max_energy;
}

double peak(const std::vector<Kakshya::DataVariant>& channels)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::PEAK);
    auto result = analyzer->analyze_energy(channels);
    double global_peak = 0.0;
    for (const auto& channel : result.channels) {
        global_peak = std::max(global_peak, channel.max_energy);
    }
    return global_peak;
}

std::vector<double> peak_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::PEAK);
    auto result = analyzer->analyze_energy(channels);
    std::vector<double> peaks;
    for (const auto& channel : result.channels) {
        peaks.push_back(channel.max_energy);
    }
    return peaks;
}

double peak_channel(const std::vector<Kakshya::DataVariant>& channels, size_t channel_index)
{
    if (channel_index >= channels.size()) {
        throw std::out_of_range("Channel index out of range");
    }
    return peak(channels[channel_index]);
}

//=========================================================================
// FEATURE EXTRACTION - Use FeatureExtractor to get ACTUAL data
//=========================================================================

std::vector<size_t> zero_crossings(const std::vector<double>& data, double threshold)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);

    auto result = analyzer->analyze_energy({ { data } });

    if (result.channels.empty()) {
        return {};
    }

    const auto& positions = result.channels[0].event_positions;

    if (threshold <= 0.0) {
        return positions;
    }

    std::vector<size_t> filtered;
    for (size_t pos : positions) {
        if (pos < data.size() && std::abs(data[pos]) >= threshold) {
            filtered.push_back(pos);
        }
    }
    return filtered;
}

std::vector<size_t> zero_crossings(const Kakshya::DataVariant& data, double threshold)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);

    auto result = analyzer->analyze_energy({ data });

    if (result.channels.empty()) {
        return {};
    }

    const auto& positions = result.channels[0].event_positions;

    if (threshold <= 0.0) {
        return positions;
    }

    auto double_data = std::get<std::vector<double>>(data);
    std::vector<size_t> filtered;
    for (size_t pos : positions) {
        if (pos < double_data.size() && std::abs(double_data[pos]) >= threshold) {
            filtered.push_back(pos);
        }
    }
    return filtered;
}

std::vector<std::vector<size_t>> zero_crossings_per_channel(const std::vector<Kakshya::DataVariant>& channels, double threshold)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    auto result = analyzer->analyze_energy(channels);

    std::vector<std::vector<size_t>> all_crossings;
    all_crossings.reserve(result.channels.size());

    for (size_t ch = 0; ch < result.channels.size(); ++ch) {
        const auto& positions = result.channels[ch].event_positions;

        if (threshold <= 0.0) {
            all_crossings.push_back(positions);
            continue;
        }

        auto double_data = std::get<std::vector<double>>(channels[ch]);
        std::vector<size_t> filtered;
        for (size_t pos : positions) {
            if (pos < double_data.size() && std::abs(double_data[pos]) >= threshold) {
                filtered.push_back(pos);
            }
        }
        all_crossings.push_back(filtered);
    }

    return all_crossings;
}

double zero_crossing_rate(const std::vector<double>& data, size_t window_size)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    if (window_size > 0) {
        analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = analyzer->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

double zero_crossing_rate(const Kakshya::DataVariant& data, size_t window_size)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    if (window_size > 0) {
        analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = analyzer->analyze_energy({ data });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

std::vector<double> zero_crossing_rate_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    if (window_size > 0) {
        analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = analyzer->analyze_energy(channels);

    std::vector<double> rates;
    for (const auto& channel : result.channels) {
        rates.push_back(channel.mean_energy);
    }
    return rates;
}

double spectral_centroid(const std::vector<double>& data, double sample_rate)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    analyzer->set_parameter("sample_rate", sample_rate);
    auto result = analyzer->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

double spectral_centroid(const Kakshya::DataVariant& data, double sample_rate)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    analyzer->set_parameter("sample_rate", sample_rate);
    auto result = analyzer->analyze_energy({ data });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

std::vector<double> spectral_centroid_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate)
{
    auto analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    analyzer->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    analyzer->set_parameter("sample_rate", sample_rate);
    auto result = analyzer->analyze_energy(channels);

    std::vector<double> centroids;
    for (const auto& channel : result.channels) {
        centroids.push_back(channel.mean_energy);
    }
    return centroids;
}

std::vector<double> detect_onsets(const std::vector<double>& data, double sample_rate, double threshold)
{
    std::span<const double> data_span(data.data(), data.size());

    auto onset_sample_positions = Yantra::find_onset_positions(
        data_span,
        1024,
        512,
        threshold);

    std::vector<double> onset_times;
    onset_times.reserve(onset_sample_positions.size());
    for (size_t sample_pos : onset_sample_positions) {
        onset_times.push_back(static_cast<double>(sample_pos) / sample_rate);
    }

    return onset_times;
}

std::vector<double> detect_onsets(const Kakshya::DataVariant& data, double sample_rate, double threshold)
{
    auto double_data = std::get<std::vector<double>>(data);
    std::span<const double> data_span(double_data.data(), double_data.size());

    auto onset_sample_positions = Yantra::find_onset_positions(
        data_span,
        1024,
        512,
        threshold);

    std::vector<double> onset_times;
    onset_times.reserve(onset_sample_positions.size());
    for (size_t sample_pos : onset_sample_positions) {
        onset_times.push_back(static_cast<double>(sample_pos) / sample_rate);
    }

    return onset_times;
}

std::vector<std::vector<double>> detect_onsets_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate, double threshold)
{
    std::vector<std::vector<double>> all_onsets;
    all_onsets.reserve(channels.size());

    for (const auto& channel : channels) {
        auto double_data = std::get<std::vector<double>>(channel);
        std::span<const double> data_span(double_data.data(), double_data.size());

        auto onset_sample_positions = Yantra::find_onset_positions(
            data_span,
            1024,
            512,
            threshold);

        std::vector<double> onset_times;
        onset_times.reserve(onset_sample_positions.size());
        for (size_t sample_pos : onset_sample_positions) {
            onset_times.push_back(static_cast<double>(sample_pos) / sample_rate);
        }

        all_onsets.push_back(onset_times);
    }

    return all_onsets;
}

//=========================================================================
// BASIC TRANSFORMATIONS - Use MathematicalTransformer
//=========================================================================

void apply_gain(std::vector<double>& data, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = transformer->apply_operation(input);

    data = std::get<std::vector<double>>(result.data[0]);
}

void apply_gain(Kakshya::DataVariant& data, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = transformer->apply_operation(input);

    data = std::get<std::vector<double>>(result.data[0]);
}

void apply_gain_channels(std::vector<Kakshya::DataVariant>& channels, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { channels };
    auto result = transformer->apply_operation(input);
    channels = result.data;
}

void apply_gain_per_channel(std::vector<Kakshya::DataVariant>& channels, const std::vector<double>& gain_factors)
{
    if (gain_factors.size() != channels.size()) {
        throw std::invalid_argument("Gain factors size must match channels size");
    }

    for (size_t i = 0; i < channels.size(); ++i) {
        apply_gain(channels[i], gain_factors[i]);
    }
}

std::vector<double> with_gain(const std::vector<double>& data, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = transformer->apply_operation(input);
    return std::get<std::vector<double>>(result.data[0]);
}

Kakshya::DataVariant with_gain(const Kakshya::DataVariant& data, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = transformer->apply_operation(input);
    return result.data[0];
}

std::vector<Kakshya::DataVariant> with_gain_channels(const std::vector<Kakshya::DataVariant>& channels, double gain_factor)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::GAIN);
    transformer->set_parameter("gain_factor", gain_factor);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { channels };
    auto result = transformer->apply_operation(input);
    return result.data;
}

void normalize(std::vector<double>& data, double target_peak)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", target_peak);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = transformer->apply_operation(input);
    data = std::get<std::vector<double>>(result.data[0]);
}

void normalize(Kakshya::DataVariant& data, double target_peak)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", target_peak);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = transformer->apply_operation(input);
    data = result.data[0];
}

void normalize_channels(std::vector<Kakshya::DataVariant>& channels, double target_peak)
{
    for (auto& channel : channels) {
        normalize(channel, target_peak);
    }
}

void normalize_together(std::vector<Kakshya::DataVariant>& channels, double target_peak)
{
    double global_peak = peak(channels);

    if (global_peak > 0.0) {
        double gain_factor = target_peak / global_peak;
        apply_gain_channels(channels, gain_factor);
    }
}

std::vector<double> normalized(const std::vector<double>& data, double target_peak)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", target_peak);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = transformer->apply_operation(input);
    return std::get<std::vector<double>>(result.data[0]);
}

Kakshya::DataVariant normalized(const Kakshya::DataVariant& data, double target_peak)
{
    auto transformer = std::make_shared<Yantra::MathematicalTransformer<>>(Yantra::MathematicalOperation::NORMALIZE);
    transformer->set_parameter("target_peak", target_peak);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = transformer->apply_operation(input);
    return result.data[0];
}

std::vector<Kakshya::DataVariant> normalized_channels(const std::vector<Kakshya::DataVariant>& channels, double target_peak)
{
    std::vector<Kakshya::DataVariant> result;
    result.reserve(channels.size());
    for (const auto& channel : channels) {
        result.push_back(normalized(channel, target_peak));
    }
    return result;
}

//=========================================================================
// TEMPORAL TRANSFORMATIONS - Use TemporalTransformer
//=========================================================================

void reverse(std::vector<double>& data)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = transformer->apply_operation(input);
    data = std::get<std::vector<double>>(result.data[0]);
}

void reverse(Kakshya::DataVariant& data)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = transformer->apply_operation(input);
    data = result.data[0];
}

void reverse_channels(std::vector<Kakshya::DataVariant>& channels)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { channels };
    auto result = transformer->apply_operation(input);
    channels = result.data;
}

std::vector<double> reversed(const std::vector<double>& data)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = transformer->apply_operation(input);
    return std::get<std::vector<double>>(result.data[0]);
}

Kakshya::DataVariant reversed(const Kakshya::DataVariant& data)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = transformer->apply_operation(input);
    return result.data[0];
}

std::vector<Kakshya::DataVariant> reversed_channels(const std::vector<Kakshya::DataVariant>& channels)
{
    auto transformer = std::make_shared<Yantra::TemporalTransformer<>>(Yantra::TemporalOperation::TIME_REVERSE);
    Yantra::IO<std::vector<Kakshya::DataVariant>> input { channels };
    auto result = transformer->apply_operation(input);
    return result.data;
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
    auto result = energy_analyzer->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels[0].energy_values;
}

std::vector<double> magnitude_spectrum(const Kakshya::DataVariant& data, size_t window_size)
{
    auto energy_analyzer = std::make_shared<Yantra::EnergyAnalyzer<>>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    if (window_size > 0) {
        energy_analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = energy_analyzer->analyze_energy({ data });
    return result.channels[0].energy_values;
}

std::vector<std::vector<double>> magnitude_spectrum_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size)
{
    auto energy_analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    if (window_size > 0) {
        energy_analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = energy_analyzer->analyze_energy(channels);

    std::vector<std::vector<double>> spectra;
    for (const auto& channel : result.channels) {
        spectra.push_back(channel.energy_values);
    }
    return spectra;
}

std::vector<double> power_spectrum(const std::vector<double>& data, size_t window_size)
{
    auto energy_analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::POWER);
    if (window_size > 0) {
        energy_analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = energy_analyzer->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels[0].energy_values;
}

std::vector<double> power_spectrum(const Kakshya::DataVariant& data, size_t window_size)
{
    auto energy_analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::POWER);
    if (window_size > 0) {
        energy_analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = energy_analyzer->analyze_energy({ data });
    return result.channels[0].energy_values;
}

std::vector<std::vector<double>> power_spectrum_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size)
{
    auto energy_analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::POWER);
    if (window_size > 0) {
        energy_analyzer->set_window_parameters(window_size, window_size / 2);
    }
    auto result = energy_analyzer->analyze_energy(channels);

    std::vector<std::vector<double>> spectra;
    for (const auto& channel : result.channels) {
        spectra.push_back(channel.energy_values);
    }
    return spectra;
}

//=========================================================================
// PITCH AND TIME - Use EnergyAnalyzer for harmonic analysis
//=========================================================================

double estimate_pitch(const std::vector<double>& data, double sample_rate, double min_freq, double max_freq)
{
    auto energy_analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::HARMONIC);
    energy_analyzer->set_parameter("sample_rate", sample_rate);
    energy_analyzer->set_parameter("min_freq", min_freq);
    energy_analyzer->set_parameter("max_freq", max_freq);
    auto result = energy_analyzer->analyze_energy({ Kakshya::DataVariant(data) });

    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy * sample_rate / 1000.0;
}

double estimate_pitch(const Kakshya::DataVariant& data, double sample_rate, double min_freq, double max_freq)
{
    auto energy_analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::HARMONIC);
    energy_analyzer->set_parameter("sample_rate", sample_rate);
    energy_analyzer->set_parameter("min_freq", min_freq);
    energy_analyzer->set_parameter("max_freq", max_freq);
    auto result = energy_analyzer->analyze_energy({ data });

    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy * sample_rate / 1000.0;
}

std::vector<double> estimate_pitch_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate, double min_freq, double max_freq)
{
    auto energy_analyzer = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    energy_analyzer->set_energy_method(Yantra::EnergyMethod::HARMONIC);
    energy_analyzer->set_parameter("sample_rate", sample_rate);
    energy_analyzer->set_parameter("min_freq", min_freq);
    energy_analyzer->set_parameter("max_freq", max_freq);
    auto result = energy_analyzer->analyze_energy(channels);

    std::vector<double> pitches;
    for (const auto& channel : result.channels) {
        double pitch = channel.mean_energy * sample_rate / 1000.0;
        pitches.push_back(pitch);
    }
    return pitches;
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

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = extractor->apply_operation(input);

    return result.data[0];
}

std::vector<double> extract_silent_data(const Kakshya::DataVariant& data,
    double threshold,
    size_t min_silence_duration)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
    extractor->set_parameter("silence_threshold", threshold);
    extractor->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = extractor->apply_operation(input);

    return result.data[0];
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

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = extractor->apply_operation(input);

    return result.data[0];
}

std::vector<double> extract_zero_crossing_regions(const Kakshya::DataVariant& data,
    double threshold,
    size_t region_size)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::ZERO_CROSSING_DATA);
    extractor->set_parameter("threshold", threshold);
    extractor->set_parameter("min_distance", 1.0);
    extractor->set_parameter("region_size", static_cast<uint32_t>(region_size));

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = extractor->apply_operation(input);

    return result.data[0];
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
    auto double_data = std::get<std::vector<double>>(data);
    apply_window(double_data, window_type);
    data = Kakshya::DataVariant(double_data);
}

void apply_window_channels(std::vector<Kakshya::DataVariant>& channels, const std::string& window_type)
{
    for (auto& channel : channels) {
        apply_window(channel, window_type);
    }
}

std::vector<std::vector<double>> windowed_segments(const std::vector<double>& data,
    size_t window_size,
    size_t hop_size)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>(window_size, hop_size,
        Yantra::ExtractionMethod::OVERLAPPING_WINDOWS);
    extractor->set_parameter("overlap", double(hop_size) / window_size);

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = extractor->apply_operation(input);

    auto extracted_data = result.data[0];

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
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>(window_size, hop_size,
        Yantra::ExtractionMethod::OVERLAPPING_WINDOWS);
    extractor->set_parameter("overlap", double(hop_size) / window_size);

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = extractor->apply_operation(input);

    auto extracted_data = result.data[0];

    std::vector<std::vector<double>> segments;
    for (size_t i = 0; i < extracted_data.size(); i += window_size) {
        size_t end_idx = std::min(i + window_size, extracted_data.size());
        segments.emplace_back(extracted_data.begin() + i, extracted_data.begin() + end_idx);
    }

    return segments;
}

std::vector<std::vector<std::vector<double>>> windowed_segments_per_channel(const std::vector<Kakshya::DataVariant>& channels,
    size_t window_size,
    size_t hop_size)
{
    std::vector<std::vector<std::vector<double>>> all_segments;
    all_segments.reserve(channels.size());

    for (const auto& channel : channels) {
        all_segments.push_back(windowed_segments(channel, window_size, hop_size));
    }

    return all_segments;
}

std::vector<std::pair<size_t, size_t>> detect_silence(const std::vector<double>& data,
    double threshold,
    size_t min_silence_duration)
{
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
    extractor->set_parameter("silence_threshold", threshold);
    extractor->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
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
    auto extractor = std::make_shared<Yantra::FeatureExtractor<>>();
    extractor->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
    extractor->set_parameter("silence_threshold", threshold);
    extractor->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));

    Yantra::IO<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = extractor->apply_operation(input);

    std::vector<std::pair<size_t, size_t>> silence_regions;
    auto window_positions_opt = result.template get_metadata<std::vector<std::pair<size_t, size_t>>>("window_positions");
    if (window_positions_opt.has_value()) {
        silence_regions = window_positions_opt.value();
    }

    return silence_regions;
}

std::vector<std::vector<std::pair<size_t, size_t>>> detect_silence_per_channel(const std::vector<Kakshya::DataVariant>& channels,
    double threshold,
    size_t min_silence_duration)
{
    std::vector<std::vector<std::pair<size_t, size_t>>> all_silence_regions;
    all_silence_regions.reserve(channels.size());

    for (const auto& channel : channels) {
        all_silence_regions.push_back(detect_silence(channel, threshold, min_silence_duration));
    }

    return all_silence_regions;
}

//=========================================================================
// UTILITY AND CONVERSION - Use MathematicalTransformer for mixing
//=========================================================================

std::vector<double> mix(const std::vector<std::vector<double>>& streams)
{
    if (streams.empty())
        return {};

    size_t max_length = 0;
    for (const auto& stream : streams) {
        max_length = std::max(max_length, stream.size());
    }

    std::vector<double> result(max_length, 0.0);

    for (const auto& stream : streams) {
        for (size_t i = 0; i < stream.size(); ++i) {
            result[i] += stream[i];
        }
    }

    double gain = 1.0 / static_cast<double>(streams.size());
    apply_gain(result, gain);

    return result;
}

std::vector<double> mix(const std::vector<Kakshya::DataVariant>& streams)
{
    if (streams.empty())
        return {};

    auto numeric_data = Yantra::OperationHelper::extract_numeric_data(streams);

    if (is_same_size(numeric_data)) {
        size_t channel_length = numeric_data[0].size();
        std::vector<double> result(channel_length, 0.0);

        for (const auto& span : numeric_data) {
            for (size_t i = 0; i < span.size(); ++i) {
                result[i] += span[i];
            }
        }

        double gain = 1.0 / static_cast<double>(numeric_data.size());
        apply_gain(result, gain);

        return result;
    } else {
        std::vector<std::vector<double>> double_streams;
        double_streams.reserve(streams.size());

        for (const auto& span : numeric_data) {
            double_streams.emplace_back(span.begin(), span.end());
        }

        return mix(double_streams);
    }
}

std::vector<double> mix_with_gains(const std::vector<std::vector<double>>& streams,
    const std::vector<double>& gains)
{
    if (streams.empty() || gains.size() != streams.size()) {
        throw std::invalid_argument("Streams and gains vectors must have the same size");
    }

    size_t max_length = 0;
    for (const auto& stream : streams) {
        max_length = std::max(max_length, stream.size());
    }

    std::vector<double> result(max_length, 0.0);

    for (size_t s = 0; s < streams.size(); ++s) {
        const auto& stream = streams[s];
        double gain = gains[s];

        for (size_t i = 0; i < stream.size(); ++i) {
            result[i] += stream[i] * gain;
        }
    }

    return result;
}

std::vector<double> mix_with_gains(const std::vector<Kakshya::DataVariant>& streams,
    const std::vector<double>& gains)
{
    if (streams.empty() || gains.size() != streams.size()) {
        throw std::invalid_argument("Streams and gains vectors must have the same size");
    }

    auto numeric_data = Yantra::OperationHelper::extract_numeric_data(streams);

    if (is_same_size(numeric_data)) {
        size_t channel_length = numeric_data[0].size();
        std::vector<double> result(channel_length, 0.0);

        for (size_t s = 0; s < numeric_data.size(); ++s) {
            const auto& span = numeric_data[s];
            double gain = gains[s];

            for (size_t i = 0; i < span.size(); ++i) {
                result[i] += span[i] * gain;
            }
        }

        return result;
    }

    std::vector<std::vector<double>> double_streams;
    double_streams.reserve(streams.size());

    for (const auto& span : numeric_data) {
        double_streams.emplace_back(span.begin(), span.end());
    }

    return mix_with_gains(double_streams, gains);
}

std::vector<double> to_double_vector(const Kakshya::DataVariant& data)
{
    auto span = Yantra::OperationHelper::extract_numeric_data(data);
    return span.empty() ? std::vector<double> {} : std::vector<double>(span.begin(), span.end());
}

Kakshya::DataVariant to_data_variant(const std::vector<double>& data)
{
    return { data };
}

std::vector<std::vector<double>> to_double_vectors(const std::vector<Kakshya::DataVariant>& channels)
{
    auto spans = Yantra::OperationHelper::extract_numeric_data(channels);
    std::vector<std::vector<double>> result;
    result.reserve(spans.size());

    for (const auto& span : spans) {
        result.emplace_back(span.begin(), span.end());
    }

    return result;
}

std::vector<Kakshya::DataVariant> to_data_variants(const std::vector<std::vector<double>>& channel_data)
{
    std::vector<Kakshya::DataVariant> result;
    result.reserve(channel_data.size());

    for (const auto& channel : channel_data) {
        result.emplace_back(channel);
    }

    return result;
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
