#include "Yantra.hpp"

#include "MayaFlux/Nodes/Generators/WindowGenerator.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"

#include "MayaFlux/Kinesis/Discrete/Analysis.hpp"
#include "MayaFlux/Kinesis/Discrete/Transform.hpp"

namespace MayaFlux {

namespace D = Kinesis::Discrete;

namespace {
    bool is_same_size(const std::vector<std::span<double>>& data)
    {
        if (data.empty())
            return true;

        const auto expected_size = data.front().size();
        return std::ranges::all_of(data,
            [&expected_size](const auto& v) { return v.size() == expected_size; });
    }

    std::vector<double> concat_vectors(const std::vector<std::span<double>>& data)
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

    std::span<double> as_span(std::vector<double>& v) noexcept
    {
        return { v.data(), v.size() };
    }

    std::span<double> as_span(Kakshya::DataVariant& v)
    {
        return Yantra::OperationHelper::extract_numeric_data(v);
    }

}

//=========================================================================
// STATISTICAL ANALYSIS
//=========================================================================

double mean(const std::vector<double>& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StatisticalAnalyzer<>>();
        a->set_method(Yantra::StatisticalMethod::MEAN);
        return a;
    }();
    return s_op->analyze_statistics({ Kakshya::DataVariant(data) }).channel_statistics[0].mean_stat;
}

double mean(const Kakshya::DataVariant& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StatisticalAnalyzer<>>();
        a->set_method(Yantra::StatisticalMethod::MEAN);
        return a;
    }();
    return s_op->analyze_statistics({ data }).channel_statistics[0].mean_stat;
}

std::vector<double> mean_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StatisticalAnalyzer<>>();
        a->set_method(Yantra::StatisticalMethod::MEAN);
        return a;
    }();
    auto result = s_op->analyze_statistics(channels);

    std::vector<double> means;
    means.reserve(result.channel_statistics.size());
    for (const auto& stats : result.channel_statistics)
        means.push_back(stats.mean_stat);
    return means;
}

double mean_combined(const std::vector<Kakshya::DataVariant>& channels)
{
    auto data = Yantra::OperationHelper::extract_numeric_data(channels);
    if (is_same_size(data)) {
        auto result = mean_per_channel(channels);
        return std::accumulate(result.begin(), result.end(), 0.0) / (double)result.size();
    }
    auto mixed = concat_vectors(data);
    return mean(mixed);
}

double rms(const std::vector<double>& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
        a->set_method(Yantra::StatisticalMethod::RMS);
        return a;
    }();
    auto result = s_op->analyze_statistics({ Kakshya::DataVariant(data) });
    return result.channel_statistics[0].statistical_values.empty() ? 0.0 : result.channel_statistics[0].statistical_values[0];
}

double rms(const Kakshya::DataVariant& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
        a->set_method(Yantra::StatisticalMethod::RMS);
        return a;
    }();
    auto result = s_op->analyze_statistics({ data });
    return result.channel_statistics[0].statistical_values.empty() ? 0.0 : result.channel_statistics[0].statistical_values[0];
}

std::vector<double> rms_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
        a->set_method(Yantra::StatisticalMethod::RMS);
        return a;
    }();
    auto result = s_op->analyze_statistics(channels);
    std::vector<double> rms_values;
    rms_values.reserve(result.channel_statistics.size());

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
        for (double v : result)
            sum_squares += v * v;
        return std::sqrt(sum_squares / (double)result.size());
    }
    auto mixed = concat_vectors(data);
    return rms(mixed);
}

double std_dev(const std::vector<double>& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
        a->set_method(Yantra::StatisticalMethod::STD_DEV);
        return a;
    }();
    return s_op->analyze_statistics({ Kakshya::DataVariant(data) }).channel_statistics[0].stat_std_dev;
}

double std_dev(const Kakshya::DataVariant& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
        a->set_method(Yantra::StatisticalMethod::STD_DEV);
        return a;
    }();
    return s_op->analyze_statistics({ data }).channel_statistics[0].stat_std_dev;
}

std::vector<double> std_dev_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardStatisticalAnalyzer>();
        a->set_method(Yantra::StatisticalMethod::STD_DEV);
        return a;
    }();
    auto result = s_op->analyze_statistics(channels);
    std::vector<double> std_devs;

    std_devs.reserve(result.channel_statistics.size());
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
        return std::accumulate(result.begin(), result.end(), 0.0) / (double)result.size();
    }
    auto mixed = concat_vectors(data);
    return std_dev(mixed);
}

//=========================================================================
// ENERGY ANALYSIS
//=========================================================================

double dynamic_range(const std::vector<double>& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::DYNAMIC_RANGE);
        return a;
    }();
    auto result = s_op->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() || result.channels[0].energy_values.empty() ? 0.0 : result.channels[0].energy_values[0];
}

double dynamic_range(const Kakshya::DataVariant& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::DYNAMIC_RANGE);
        return a;
    }();
    auto result = s_op->analyze_energy({ data });
    return result.channels.empty() || result.channels[0].energy_values.empty() ? 0.0 : result.channels[0].energy_values[0];
}

std::vector<double> dynamic_range_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::DYNAMIC_RANGE);
        return a;
    }();
    auto result = s_op->analyze_energy(channels);
    std::vector<double> ranges;

    ranges.reserve(result.channels.size());
    for (const auto& ch : result.channels) {
        ranges.push_back(ch.energy_values.empty() ? 0.0 : ch.energy_values[0]);
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

    if (global_min <= 0.0 || global_max <= 0.0)
        return 0.0;
    return 20.0 * std::log10(global_max / std::abs(global_min));
}

double peak(const std::vector<double>& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::PEAK);
        return a;
    }();
    auto result = s_op->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() ? 0.0 : result.channels[0].max_energy;
}

double peak(const Kakshya::DataVariant& data)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::PEAK);
        return a;
    }();
    auto result = s_op->analyze_energy({ data });
    return result.channels.empty() ? 0.0 : result.channels[0].max_energy;
}

double peak(const std::vector<Kakshya::DataVariant>& channels)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::PEAK);
        return a;
    }();
    auto result = s_op->analyze_energy(channels);
    double global_peak = 0.0;
    for (const auto& ch : result.channels)
        global_peak = std::max(global_peak, ch.max_energy);
    return global_peak;
}

std::vector<double> peak_per_channel(const std::vector<Kakshya::DataVariant>& channels)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::PEAK);
        return a;
    }();
    auto result = s_op->analyze_energy(channels);
    std::vector<double> peaks;
    peaks.reserve(result.channels.size());

    for (const auto& ch : result.channels)
        peaks.push_back(ch.max_energy);
    return peaks;
}

double peak_channel(const std::vector<Kakshya::DataVariant>& channels, size_t channel_index)
{
    if (channel_index >= channels.size()) {
        error<std::out_of_range>(Journal::Component::API, Journal::Context::API, std::source_location::current(),
            "Channel index out of range");
    }
    return peak(channels[channel_index]);
}

//=========================================================================
// FEATURE EXTRACTION
//=========================================================================

std::vector<size_t> zero_crossings(const std::vector<double>& data, double threshold)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
        return a;
    }();
    auto result = s_op->analyze_energy({ Kakshya::DataVariant(data) });
    if (result.channels.empty())
        return {};

    const auto& positions = result.channels[0].event_positions;
    if (threshold <= 0.0)
        return positions;

    std::vector<size_t> filtered;
    for (size_t pos : positions) {
        if (pos < data.size() && std::abs(data[pos]) >= threshold)
            filtered.push_back(pos);
    }
    return filtered;
}

std::vector<size_t> zero_crossings(const Kakshya::DataVariant& data, double threshold)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
        return a;
    }();
    auto result = s_op->analyze_energy({ data });
    if (result.channels.empty())
        return {};

    const auto& positions = result.channels[0].event_positions;
    if (threshold <= 0.0)
        return positions;

    auto double_data = std::get<std::vector<double>>(data);
    std::vector<size_t> filtered;
    for (size_t pos : positions) {
        if (pos < double_data.size() && std::abs(double_data[pos]) >= threshold)
            filtered.push_back(pos);
    }
    return filtered;
}

std::vector<std::vector<size_t>> zero_crossings_per_channel(const std::vector<Kakshya::DataVariant>& channels, double threshold)
{
    static const auto s_op = [] {
        auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
        a->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
        return a;
    }();
    auto result = s_op->analyze_energy(channels);
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
            if (pos < double_data.size() && std::abs(double_data[pos]) >= threshold)
                filtered.push_back(pos);
        }
        all_crossings.push_back(filtered);
    }
    return all_crossings;
}

double zero_crossing_rate(const std::vector<double>& data, size_t window_size)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    auto result = a->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

double zero_crossing_rate(const Kakshya::DataVariant& data, size_t window_size)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    auto result = a->analyze_energy({ data });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

std::vector<double> zero_crossing_rate_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::ZERO_CROSSING);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    auto result = a->analyze_energy(channels);
    std::vector<double> rates;

    rates.reserve(result.channels.size());
    for (const auto& ch : result.channels)
        rates.push_back(ch.mean_energy);
    return rates;
}

double spectral_centroid(const std::vector<double>& data, double sample_rate)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    a->set_parameter("sample_rate", sample_rate);
    auto result = a->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

double spectral_centroid(const Kakshya::DataVariant& data, double sample_rate)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    a->set_parameter("sample_rate", sample_rate);
    auto result = a->analyze_energy({ data });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy;
}

std::vector<double> spectral_centroid_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    a->set_parameter("sample_rate", sample_rate);
    auto result = a->analyze_energy(channels);
    std::vector<double> centroids;

    centroids.reserve(result.channels.size());
    for (const auto& ch : result.channels)
        centroids.push_back(ch.mean_energy);

    return centroids;
}

std::vector<double> detect_onsets(const std::vector<double>& data, double sample_rate, double threshold)
{
    auto onset_positions = Kinesis::Discrete::onset_positions(
        std::span<const double>(data.data(), data.size()), 1024, 512, threshold);
    std::vector<double> times;

    times.reserve(onset_positions.size());
    for (size_t pos : onset_positions) {
        times.push_back(static_cast<double>(pos) / sample_rate);
    }
    return times;
}

std::vector<double> detect_onsets(const Kakshya::DataVariant& data, double sample_rate, double threshold)
{
    auto double_data = std::get<std::vector<double>>(data);
    auto onset_positions = Kinesis::Discrete::onset_positions(
        std::span<const double>(double_data.data(), double_data.size()), 1024, 512, threshold);

    std::vector<double> times;

    times.reserve(onset_positions.size());
    for (size_t pos : onset_positions)
        times.push_back(static_cast<double>(pos) / sample_rate);
    return times;
}

std::vector<std::vector<double>> detect_onsets_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate, double threshold)
{
    std::vector<std::vector<double>> all_onsets;
    all_onsets.reserve(channels.size());

    for (const auto& channel : channels)
        all_onsets.push_back(detect_onsets(channel, sample_rate, threshold));
    return all_onsets;
}

//=========================================================================
// BASIC TRANSFORMATIONS - Use Kinesis
//=========================================================================

void apply_gain(std::vector<double>& data, double gain_factor)
{
    D::linear(as_span(data), gain_factor, 0.0);
}

void apply_gain(Kakshya::DataVariant& data, double gain_factor)
{
    D::linear(as_span(data), gain_factor, 0.0);
}

void apply_gain_channels(std::vector<Kakshya::DataVariant>& channels, double gain_factor)
{
    for (auto& ch : channels)
        apply_gain(ch, gain_factor);
}

void apply_gain_per_channel(std::vector<Kakshya::DataVariant>& channels, const std::vector<double>& gain_factors)
{
    if (gain_factors.size() != channels.size()) {
        error<std::invalid_argument>(Journal::Component::API, Journal::Context::API, std::source_location::current(),
            "Gain factors size must match channels size");
    }
    for (size_t i = 0; i < channels.size(); ++i)
        apply_gain(channels[i], gain_factors[i]);
}

std::vector<double> with_gain(const std::vector<double>& data, double gain_factor)
{
    auto out = data;
    D::linear(as_span(out), gain_factor, 0.0);
    return out;
}

Kakshya::DataVariant with_gain(const Kakshya::DataVariant& data, double gain_factor)
{
    auto out = data;
    D::linear(as_span(out), gain_factor, 0.0);
    return out;
}

std::vector<Kakshya::DataVariant> with_gain_channels(const std::vector<Kakshya::DataVariant>& channels, double gain_factor)
{
    auto out = channels;
    for (auto& ch : out)
        D::linear(as_span(ch), gain_factor, 0.0);
    return out;
}

void normalize(std::vector<double>& data, double target_peak)
{
    D::normalize(as_span(data), -target_peak, target_peak);
}

void normalize(Kakshya::DataVariant& data, double target_peak)
{
    D::normalize(as_span(data), -target_peak, target_peak);
}

void normalize_channels(std::vector<Kakshya::DataVariant>& channels, double target_peak)
{
    for (auto& ch : channels)
        normalize(ch, target_peak);
}

void normalize_together(std::vector<Kakshya::DataVariant>& channels, double target_peak)
{
    const double global_peak = peak(channels);
    if (global_peak > 0.0)
        apply_gain_channels(channels, target_peak / global_peak);
}

std::vector<double> normalized(const std::vector<double>& data, double target_peak)
{
    auto out = data;
    D::normalize(as_span(out), -target_peak, target_peak);
    return out;
}

Kakshya::DataVariant normalized(const Kakshya::DataVariant& data, double target_peak)
{
    auto out = data;
    D::normalize(as_span(out), -target_peak, target_peak);
    return out;
}

std::vector<Kakshya::DataVariant> normalized_channels(const std::vector<Kakshya::DataVariant>& channels, double target_peak)
{
    auto out = channels;
    for (auto& ch : out)
        normalize(ch, target_peak);
    return out;
}

//=========================================================================
// TEMPORAL TRANSFORMATIONS
//=========================================================================

void reverse(std::vector<double>& data)
{
    D::reverse(as_span(data));
}

void reverse(Kakshya::DataVariant& data)
{
    D::reverse(as_span(data));
}

void reverse_channels(std::vector<Kakshya::DataVariant>& channels)
{
    for (auto& ch : channels)
        D::reverse(as_span(ch));
}

std::vector<double> reversed(const std::vector<double>& data)
{
    auto out = data;
    D::reverse(as_span(out));
    return out;
}

Kakshya::DataVariant reversed(const Kakshya::DataVariant& data)
{
    auto out = data;
    D::reverse(as_span(out));
    return out;
}

std::vector<Kakshya::DataVariant> reversed_channels(const std::vector<Kakshya::DataVariant>& channels)
{
    auto out = channels;
    for (auto& ch : out)
        D::reverse(as_span(ch));
    return out;
}

//=========================================================================
// FREQUENCY DOMAIN
//=========================================================================

std::vector<double> magnitude_spectrum(const std::vector<double>& data, size_t window_size)
{
    auto a = std::make_shared<Yantra::EnergyAnalyzer<>>();
    a->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    return a->analyze_energy({ Kakshya::DataVariant(data) }).channels[0].energy_values;
}

std::vector<double> magnitude_spectrum(const Kakshya::DataVariant& data, size_t window_size)
{
    auto a = std::make_shared<Yantra::EnergyAnalyzer<>>();
    a->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    return a->analyze_energy({ data }).channels[0].energy_values;
}

std::vector<std::vector<double>> magnitude_spectrum_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::SPECTRAL);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    auto result = a->analyze_energy(channels);
    std::vector<std::vector<double>> spectra;

    spectra.reserve(result.channels.size());
    for (const auto& ch : result.channels) {
        spectra.push_back(ch.energy_values);
    }
    return spectra;
}

std::vector<double> power_spectrum(const std::vector<double>& data, size_t window_size)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::POWER);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    return a->analyze_energy({ Kakshya::DataVariant(data) }).channels[0].energy_values;
}

std::vector<double> power_spectrum(const Kakshya::DataVariant& data, size_t window_size)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::POWER);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    return a->analyze_energy({ data }).channels[0].energy_values;
}

std::vector<std::vector<double>> power_spectrum_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::POWER);
    if (window_size > 0)
        a->set_window_parameters(window_size, window_size / 2);
    auto result = a->analyze_energy(channels);
    std::vector<std::vector<double>> spectra;

    spectra.reserve(result.channels.size());
    for (const auto& ch : result.channels) {
        spectra.push_back(ch.energy_values);
    }

    return spectra;
}

//=========================================================================
// PITCH AND TIME - Use EnergyAnalyzer for harmonic analysis
//=========================================================================

double estimate_pitch(const std::vector<double>& data, double sample_rate, double min_freq, double max_freq)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::HARMONIC);
    a->set_parameter("sample_rate", sample_rate);
    a->set_parameter("min_freq", min_freq);
    a->set_parameter("max_freq", max_freq);
    auto result = a->analyze_energy({ Kakshya::DataVariant(data) });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy * sample_rate / 1000.0;
}

double estimate_pitch(const Kakshya::DataVariant& data, double sample_rate, double min_freq, double max_freq)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::HARMONIC);
    a->set_parameter("sample_rate", sample_rate);
    a->set_parameter("min_freq", min_freq);
    a->set_parameter("max_freq", max_freq);
    auto result = a->analyze_energy({ data });
    return result.channels.empty() ? 0.0 : result.channels[0].mean_energy * sample_rate / 1000.0;
}

std::vector<double> estimate_pitch_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate, double min_freq, double max_freq)
{
    auto a = std::make_shared<Yantra::StandardEnergyAnalyzer>();
    a->set_energy_method(Yantra::EnergyMethod::HARMONIC);
    a->set_parameter("sample_rate", sample_rate);
    a->set_parameter("min_freq", min_freq);
    a->set_parameter("max_freq", max_freq);
    auto result = a->analyze_energy(channels);
    std::vector<double> pitches;
    pitches.reserve(result.channels.size());

    for (const auto& ch : result.channels) {
        pitches.push_back(ch.mean_energy * sample_rate / 1000.0);
    }
    return pitches;
}

//=========================================================================
// WINDOWING AND SEGMENTATION
//=========================================================================

std::vector<double> extract_silent_data(const std::vector<double>& data, double threshold, size_t min_silence_duration)
{
    static const auto s_op = [] {
        auto e = std::make_shared<Yantra::FeatureExtractor<>>();
        e->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
        return e;
    }();
    s_op->set_parameter("silence_threshold", threshold);
    s_op->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));
    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    return s_op->apply_operation(input).data[0];
}

std::vector<double> extract_silent_data(const Kakshya::DataVariant& data, double threshold, size_t min_silence_duration)
{
    static const auto s_op = [] {
        auto e = std::make_shared<Yantra::FeatureExtractor<>>();
        e->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
        return e;
    }();
    s_op->set_parameter("silence_threshold", threshold);
    s_op->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));
    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { data } };
    return s_op->apply_operation(input).data[0];
}

std::vector<double> extract_zero_crossing_regions(const std::vector<double>& data, double threshold, size_t region_size)
{
    static const auto s_op = [] {
        auto e = std::make_shared<Yantra::FeatureExtractor<>>();
        e->set_extraction_method(Yantra::ExtractionMethod::ZERO_CROSSING_DATA);
        e->set_parameter("min_distance", 1.0);
        return e;
    }();
    s_op->set_parameter("threshold", threshold);
    s_op->set_parameter("region_size", static_cast<uint32_t>(region_size));
    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    return s_op->apply_operation(input).data[0];
}

std::vector<double> extract_zero_crossing_regions(const Kakshya::DataVariant& data, double threshold, size_t region_size)
{
    static const auto s_op = [] {
        auto e = std::make_shared<Yantra::FeatureExtractor<>>();
        e->set_extraction_method(Yantra::ExtractionMethod::ZERO_CROSSING_DATA);
        e->set_parameter("min_distance", 1.0);
        return e;
    }();
    s_op->set_parameter("threshold", threshold);
    s_op->set_parameter("region_size", static_cast<uint32_t>(region_size));
    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { data } };
    return s_op->apply_operation(input).data[0];
}

void apply_window(std::vector<double>& data, const std::string& window_type)
{
    Nodes::Generator::WindowType win_type {};

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
    for (size_t i = 0; i < data.size() && i < window.size(); ++i)
        data[i] *= window[i];
}

void apply_window(Kakshya::DataVariant& data, const std::string& window_type)
{
    auto double_data = std::get<std::vector<double>>(data);
    apply_window(double_data, window_type);
    data = Kakshya::DataVariant(double_data);
}

void apply_window_channels(std::vector<Kakshya::DataVariant>& channels, const std::string& window_type)
{
    for (auto& ch : channels)
        apply_window(ch, window_type);
}

std::vector<std::vector<double>> windowed_segments(const std::vector<double>& data, size_t window_size, size_t hop_size)
{
    auto e = std::make_shared<Yantra::FeatureExtractor<>>(
        static_cast<uint32_t>(window_size), static_cast<uint32_t>(hop_size),
        Yantra::ExtractionMethod::OVERLAPPING_WINDOWS);
    e->set_parameter("overlap", double(hop_size) / (double)window_size);

    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto extracted = e->apply_operation(input).data[0];
    std::vector<std::vector<double>> segments;
    for (size_t i = 0; i < extracted.size(); i += window_size) {
        size_t end = std::min(i + window_size, extracted.size());
        segments.emplace_back(extracted.begin() + i, extracted.begin() + end);
    }
    return segments;
}

std::vector<std::vector<double>> windowed_segments(const Kakshya::DataVariant& data, size_t window_size, size_t hop_size)
{
    auto e = std::make_shared<Yantra::FeatureExtractor<>>(
        static_cast<uint32_t>(window_size), static_cast<uint32_t>(hop_size),
        Yantra::ExtractionMethod::OVERLAPPING_WINDOWS);
    e->set_parameter("overlap", double(hop_size) / (double)window_size);
    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { data } };
    auto extracted = e->apply_operation(input).data[0];
    std::vector<std::vector<double>> segments;
    for (size_t i = 0; i < extracted.size(); i += window_size) {
        size_t end = std::min(i + window_size, extracted.size());
        segments.emplace_back(extracted.begin() + i, extracted.begin() + end);
    }
    return segments;
}

std::vector<std::vector<std::vector<double>>> windowed_segments_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size, size_t hop_size)
{
    std::vector<std::vector<std::vector<double>>> all_segments;
    all_segments.reserve(channels.size());
    for (const auto& ch : channels)
        all_segments.push_back(windowed_segments(ch, window_size, hop_size));
    return all_segments;
}

std::vector<std::pair<size_t, size_t>> detect_silence(const std::vector<double>& data, double threshold, size_t min_silence_duration)
{
    static const auto s_op = [] {
        auto e = std::make_shared<Yantra::FeatureExtractor<>>();
        e->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
        return e;
    }();
    s_op->set_parameter("silence_threshold", threshold);
    s_op->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));
    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { Kakshya::DataVariant(data) } };
    auto result = s_op->apply_operation(input);
    auto pos = result.template get_metadata<std::vector<std::pair<size_t, size_t>>>("window_positions");
    return pos.has_value() ? pos.value() : std::vector<std::pair<size_t, size_t>> {};
}

std::vector<std::pair<size_t, size_t>> detect_silence(const Kakshya::DataVariant& data, double threshold, size_t min_silence_duration)
{
    static const auto s_op = [] {
        auto e = std::make_shared<Yantra::FeatureExtractor<>>();
        e->set_extraction_method(Yantra::ExtractionMethod::SILENCE_DATA);
        return e;
    }();
    s_op->set_parameter("silence_threshold", threshold);
    s_op->set_parameter("min_duration", static_cast<uint32_t>(min_silence_duration));
    Yantra::Datum<std::vector<Kakshya::DataVariant>> input { { data } };
    auto result = s_op->apply_operation(input);
    auto pos = result.template get_metadata<std::vector<std::pair<size_t, size_t>>>("window_positions");
    return pos.has_value() ? pos.value() : std::vector<std::pair<size_t, size_t>> {};
}

std::vector<std::vector<std::pair<size_t, size_t>>> detect_silence_per_channel(const std::vector<Kakshya::DataVariant>& channels, double threshold, size_t min_silence_duration)
{
    std::vector<std::vector<std::pair<size_t, size_t>>> all_regions;
    all_regions.reserve(channels.size());
    for (const auto& ch : channels)
        all_regions.push_back(detect_silence(ch, threshold, min_silence_duration));
    return all_regions;
}

//=========================================================================
// UTILITY AND CONVERSION - Use MathematicalTransformer for mixing
//=========================================================================

std::vector<double> mix(const std::vector<std::vector<double>>& streams)
{
    if (streams.empty())
        return {};

    size_t max_length = 0;
    for (const auto& s : streams)
        max_length = std::max(max_length, s.size());

    std::vector<double> result(max_length, 0.0);
    for (const auto& s : streams) {
        for (size_t i = 0; i < s.size(); ++i)
            result[i] += s[i];
    }

    apply_gain(result, 1.0 / static_cast<double>(streams.size()));
    return result;
}

std::vector<double> mix(const std::vector<Kakshya::DataVariant>& streams)
{
    if (streams.empty())
        return {};

    auto numeric_data = Yantra::OperationHelper::extract_numeric_data(streams);

    if (is_same_size(numeric_data)) {
        std::vector<double> result(numeric_data[0].size(), 0.0);
        for (const auto& span : numeric_data) {
            for (size_t i = 0; i < span.size(); ++i)
                result[i] += span[i];
        }
        apply_gain(result, 1.0 / static_cast<double>(numeric_data.size()));
        return result;
    }

    std::vector<std::vector<double>> double_streams;
    double_streams.reserve(streams.size());
    for (const auto& span : numeric_data)
        double_streams.emplace_back(span.begin(), span.end());
    return mix(double_streams);
}

std::vector<double> mix_with_gains(const std::vector<std::vector<double>>& streams, const std::vector<double>& gains)
{
    if (streams.empty() || gains.size() != streams.size()) {
        error<std::invalid_argument>(Journal::Component::API, Journal::Context::API, std::source_location::current(),
            "Streams and gains vectors must have the same size");
    }

    size_t max_length = 0;
    for (const auto& s : streams)
        max_length = std::max(max_length, s.size());

    std::vector<double> result(max_length, 0.0);
    for (size_t s = 0; s < streams.size(); ++s) {
        for (size_t i = 0; i < streams[s].size(); ++i)
            result[i] += streams[s][i] * gains[s];
    }
    return result;
}

std::vector<double> mix_with_gains(const std::vector<Kakshya::DataVariant>& streams, const std::vector<double>& gains)
{
    if (streams.empty() || gains.size() != streams.size()) {
        error<std::invalid_argument>(Journal::Component::API, Journal::Context::API, std::source_location::current(),
            "Streams and gains vectors must have the same size");
    }

    auto numeric_data = Yantra::OperationHelper::extract_numeric_data(streams);

    if (is_same_size(numeric_data)) {
        std::vector<double> result(numeric_data[0].size(), 0.0);
        for (size_t s = 0; s < numeric_data.size(); ++s) {
            for (size_t i = 0; i < numeric_data[s].size(); ++i)
                result[i] += numeric_data[s][i] * gains[s];
        }
        return result;
    }

    std::vector<std::vector<double>> double_streams;
    double_streams.reserve(streams.size());
    for (const auto& span : numeric_data)
        double_streams.emplace_back(span.begin(), span.end());
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
    for (const auto& span : spans)
        result.emplace_back(span.begin(), span.end());
    return result;
}

std::vector<Kakshya::DataVariant> to_data_variants(const std::vector<std::vector<double>>& channel_data)
{
    std::vector<Kakshya::DataVariant> result;
    result.reserve(channel_data.size());
    for (const auto& ch : channel_data)
        result.emplace_back(ch);
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
