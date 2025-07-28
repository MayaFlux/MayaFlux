#include "Yantra.hpp"

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/ComputeMatrix.hpp"
#include "MayaFlux/Yantra/Extractors/Extractors.hpp"
#include "MayaFlux/Yantra/Sorters/SorterHelpers.hpp"
#include "MayaFlux/Yantra/Sorters/UniversalSorter.hpp"
#include "MayaFlux/Yantra/YantraUtils.hpp"

namespace MayaFlux {

//-------------------------------------------------------------------------
// Internal Helper Functions
//-------------------------------------------------------------------------

namespace {
    /**
     * @brief Gets or creates the default compute matrix
     */
    std::shared_ptr<Yantra::ComputeMatrix> get_default_compute_matrix()
    {
        static std::shared_ptr<Yantra::ComputeMatrix> matrix = nullptr;
        if (!matrix) {
            matrix = std::make_shared<Yantra::ComputeMatrix>();
            matrix->register_analyzer<Yantra::StatisticalAnalyzer>("statistical");
            matrix->register_analyzer<Yantra::EnergyAnalyzer>("energy");
        }
        return matrix;
    }

    /**
     * @brief Execute analysis using the compute matrix
     */
    template <typename AnalyzerType>
    double execute_analysis(const Yantra::AnalyzerInput& input, const std::string& method)
    {
        auto matrix = get_default_compute_matrix();
        auto analyzer = matrix->create_operation<AnalyzerType>(
            std::is_same_v<AnalyzerType, Yantra::StatisticalAnalyzer> ? "statistical" : "energy");

        if (!analyzer) {
            throw std::runtime_error("Analyzer not available");
        }

        analyzer->set_parameter("method", method);
        auto result = analyzer->apply_operation(input);

        if (auto values = std::get_if<std::vector<double>>(&result)) {
            if (!values->empty()) {
                return (*values)[0];
            }
        }

        throw std::runtime_error("Analysis failed to produce result");
    }
}

//-------------------------------------------------------------------------
// Core Statistical Functions
//-------------------------------------------------------------------------

double get_mean(const std::vector<double>& data)
{
    if (data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    return execute_analysis<Yantra::StatisticalAnalyzer>(input, "mean");
}

double get_variance(const std::vector<double>& data, bool sample_corrected)
{
    if (data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    auto matrix = get_default_compute_matrix();
    auto analyzer = matrix->create_operation<Yantra::StatisticalAnalyzer>("statistical");

    analyzer->set_parameter("method", "variance");
    analyzer->set_parameter("sample_corrected", sample_corrected);

    auto result = analyzer->apply_operation(input);
    if (auto values = std::get_if<std::vector<double>>(&result)) {
        if (!values->empty())
            return (*values)[0];
    }

    return 0.0;
}

double get_std_dev(const std::vector<double>& data, bool sample_corrected)
{
    if (data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    auto matrix = get_default_compute_matrix();
    auto analyzer = matrix->create_operation<Yantra::StatisticalAnalyzer>("statistical");

    analyzer->set_parameter("method", "std_dev");
    analyzer->set_parameter("sample_corrected", sample_corrected);

    auto result = analyzer->apply_operation(input);
    if (auto values = std::get_if<std::vector<double>>(&result)) {
        if (!values->empty())
            return (*values)[0];
    }

    return 0.0;
}

double get_median(const std::vector<double>& data)
{
    if (data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    return execute_analysis<Yantra::StatisticalAnalyzer>(input, "median");
}

double get_percentile(const std::vector<double>& data, double percentile)
{
    if (data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    auto matrix = get_default_compute_matrix();
    auto analyzer = matrix->create_operation<Yantra::StatisticalAnalyzer>("statistical");

    analyzer->set_parameter("method", "percentile");
    analyzer->set_parameter("percentile_value", percentile);

    auto result = analyzer->apply_operation(input);
    if (auto values = std::get_if<std::vector<double>>(&result)) {
        if (!values->empty())
            return (*values)[0];
    }

    return 0.0;
}

double get_skewness(const std::vector<double>& data)
{
    if (data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    return execute_analysis<Yantra::StatisticalAnalyzer>(input, "skewness");
}

double get_kurtosis(const std::vector<double>& data)
{
    if (data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    return execute_analysis<Yantra::StatisticalAnalyzer>(input, "kurtosis");
}

DataSummary get_summary(const std::vector<double>& data)
{
    DataSummary summary;

    if (data.empty()) {
        return summary;
    }

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    auto matrix = get_default_compute_matrix();
    auto analyzer = matrix->create_operation<Yantra::StatisticalAnalyzer>("statistical");

    analyzer->set_parameter("method", "summary");
    auto result = analyzer->apply_operation(input);

    if (auto values = std::get_if<std::vector<double>>(&result)) {
        // Assuming the analyzer returns values in a specific order
        if (values->size() >= 9) {
            summary.mean = (*values)[0];
            summary.variance = (*values)[1];
            summary.std_dev = (*values)[2];
            summary.median = (*values)[3];
            summary.min = (*values)[4];
            summary.max = (*values)[5];
            summary.range = (*values)[6];
            summary.skewness = (*values)[7];
            summary.kurtosis = (*values)[8];
        }
    }

    return summary;
}

//-------------------------------------------------------------------------
// Sorting Operations
//-------------------------------------------------------------------------

void sort_ascending(std::vector<double>& data, Yantra::SortingAlgorithm algorithm)
{
    Yantra::sort_container(data, Yantra::SortDirection::ASCENDING, algorithm);
}

void sort_descending(std::vector<double>& data, Yantra::SortingAlgorithm algorithm)
{
    Yantra::sort_container(data, Yantra::SortDirection::DESCENDING, algorithm);
}

void sort_by_magnitude(std::vector<std::complex<double>>& data,
    Yantra::SortDirection direction, Yantra::SortingAlgorithm algorithm)
{
    Yantra::sort_complex_container(data, direction, algorithm);
}

std::vector<size_t> get_sort_indices(const std::vector<double>& data, Yantra::SortDirection direction)
{
    auto matrix = get_default_compute_matrix();

    Yantra::SorterInput input { data };

    auto sorter = matrix->create_operation<Yantra::UniversalSorter>("universal");
    sorter->set_parameter("method", "indices");
    sorter->set_parameter("direction", static_cast<int>(direction));

    auto result = sorter->apply_operation(input);

    if (auto indices = std::get_if<std::vector<size_t>>(&result)) {
        return *indices;
    }

    std::vector<size_t> indices(data.size());
    std::iota(indices.begin(), indices.end(), 0);

    if (direction == Yantra::SortDirection::ASCENDING) {
        std::sort(indices.begin(), indices.end(),
            [&data](size_t a, size_t b) { return data[a] < data[b]; });
    } else {
        std::sort(indices.begin(), indices.end(),
            [&data](size_t a, size_t b) { return data[a] > data[b]; });
    }

    return indices;
}

void sort_by_duration(std::vector<Kakshya::RegionSegment>& segments,
    Yantra::SortDirection direction, Yantra::SortingAlgorithm algorithm)
{
    Yantra::sort_segments(segments, direction, algorithm);
}

void sort_by_position(std::vector<Kakshya::Region>& regions,
    Yantra::SortDirection direction, Yantra::SortingAlgorithm algorithm)
{
    Yantra::sort_regions(regions, direction, algorithm);
}

//-------------------------------------------------------------------------
// Signal Analysis Functions
//-------------------------------------------------------------------------

double get_spectral_centroid(const std::vector<double>& audio_data)
{
    if (audio_data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(audio_data);
    return execute_analysis<Yantra::EnergyAnalyzer>(input, "spectral_centroid");
}

double get_spectral_centroid(const std::vector<std::complex<double>>& spectrum)
{
    if (spectrum.empty())
        return 0.0;

    std::vector<double> magnitudes;
    magnitudes.reserve(spectrum.size());
    for (const auto& bin : spectrum) {
        magnitudes.push_back(std::abs(bin));
    }

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(magnitudes);
    return execute_analysis<Yantra::EnergyAnalyzer>(input, "spectral_centroid");
}

double get_spectral_rolloff(const std::vector<double>& audio_data, double rolloff_threshold)
{
    if (audio_data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(audio_data);
    auto matrix = get_default_compute_matrix();
    auto analyzer = matrix->create_operation<Yantra::EnergyAnalyzer>("energy");

    analyzer->set_parameter("method", "spectral_rolloff");
    analyzer->set_parameter("rolloff_threshold", rolloff_threshold);

    auto result = analyzer->apply_operation(input);
    if (auto values = std::get_if<std::vector<double>>(&result)) {
        if (!values->empty())
            return (*values)[0];
    }

    return 0.0;
}

double get_zero_crossing_rate(const std::vector<double>& audio_data)
{
    if (audio_data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(audio_data);
    return execute_analysis<Yantra::EnergyAnalyzer>(input, "zero_crossing");
}

double get_rms_level(const std::vector<double>& audio_data)
{
    if (audio_data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(audio_data);
    return execute_analysis<Yantra::EnergyAnalyzer>(input, "rms");
}

double get_energy(const std::vector<double>& audio_data)
{
    if (audio_data.empty())
        return 0.0;

    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(audio_data);
    return execute_analysis<Yantra::EnergyAnalyzer>(input, "power");
}

double analyze_signal_container(std::shared_ptr<Kakshya::SignalSourceContainer> container,
    const std::string& method)
{
    if (!container) {
        throw std::invalid_argument("Signal container is null");
    }

    Yantra::AnalyzerInput input { container };

    if (method == "rms" || method == "energy" || method == "spectral_centroid" || method == "spectral_rolloff" || method == "zero_crossing") {
        return execute_analysis<Yantra::EnergyAnalyzer>(input, method);
    } else {
        return execute_analysis<Yantra::StatisticalAnalyzer>(input, method);
    }
}

double analyze_region(const Kakshya::Region& region, const std::string& method)
{
    Yantra::AnalyzerInput input { region };

    if (method == "rms" || method == "energy" || method == "spectral_centroid" || method == "spectral_rolloff" || method == "zero_crossing") {
        return execute_analysis<Yantra::EnergyAnalyzer>(input, method);
    } else {
        return execute_analysis<Yantra::StatisticalAnalyzer>(input, method);
    }
}

//-------------------------------------------------------------------------
// Feature Extraction
//-------------------------------------------------------------------------

std::vector<size_t> extract_peaks(const std::vector<double>& data, double threshold, size_t min_distance)
{
    if (data.empty())
        return {};

    auto matrix = get_default_compute_matrix();

    Kakshya::DataVariant data_variant { data };
    Yantra::ExtractorInput input { data_variant };

    auto extractor = matrix->create_operation<Yantra::UniversalExtractor>("peaks");

    extractor->set_parameter("method", "peaks");
    extractor->set_parameter("threshold", threshold);
    extractor->set_parameter("min_distance", static_cast<double>(min_distance));

    auto result = extractor->apply_operation(input);

    if (auto indices_double = std::get_if<std::vector<double>>(&result.base_output)) {
        std::vector<size_t> indices;
        indices.reserve(indices_double->size());
        for (double idx : *indices_double) {
            indices.push_back(static_cast<size_t>(idx));
        }
        return indices;
    }

    if (auto data_variant = std::get_if<Kakshya::DataVariant>(&result.base_output)) {
        // Check if DataVariant contains indices (this would depend on DataVariant implementation)
        // For now, fallback to empty vector
    }

    return {};
}

std::vector<size_t> extract_onsets(const std::vector<double>& data, double sensitivity)
{
    if (data.empty())
        return {};

    auto matrix = get_default_compute_matrix();

    Kakshya::DataVariant data_variant { data };
    Yantra::ExtractorInput input { data_variant };

    auto extractor = matrix->create_operation<Yantra::UniversalExtractor>("onsets");

    extractor->set_parameter("method", "onsets");
    extractor->set_parameter("sensitivity", sensitivity);

    auto result = extractor->apply_operation(input);

    if (auto indices_double = std::get_if<std::vector<double>>(&result.base_output)) {
        std::vector<size_t> indices;
        indices.reserve(indices_double->size());
        for (double idx : *indices_double) {
            indices.push_back(static_cast<size_t>(idx));
        }
        return indices;
    }

    return {};
}

std::vector<double> extract_envelope(const std::vector<double>& data, size_t window_size)
{
    if (data.empty())
        return {};

    auto matrix = get_default_compute_matrix();

    Kakshya::DataVariant data_variant { data };
    Yantra::ExtractorInput input { data_variant };

    auto extractor = matrix->create_operation<Yantra::UniversalExtractor>("envelope");

    extractor->set_parameter("method", "envelope");
    extractor->set_parameter("window_size", static_cast<double>(window_size));

    auto result = extractor->apply_operation(input);

    if (auto envelope = std::get_if<std::vector<double>>(&result.base_output)) {
        return *envelope;
    }

    return {};
}

std::vector<double> extract_above_threshold(const std::vector<double>& data, double threshold)
{
    if (data.empty())
        return {};

    auto matrix = get_default_compute_matrix();

    Kakshya::DataVariant data_variant { data };
    Yantra::ExtractorInput input { data_variant };

    auto extractor = matrix->create_operation<Yantra::UniversalExtractor>("filter");

    extractor->set_parameter("method", "above_threshold");
    extractor->set_parameter("threshold", threshold);

    auto result = extractor->apply_operation(input);

    if (auto filtered = std::get_if<std::vector<double>>(&result.base_output)) {
        return *filtered;
    }

    return {};
}

//-------------------------------------------------------------------------
// Frequency Domain Operations
//-------------------------------------------------------------------------

std::vector<std::complex<double>> get_fft(const std::vector<double>& data)
{
    if (data.empty())
        return {};

    auto matrix = get_default_compute_matrix();

    Kakshya::DataVariant data_variant { data };
    Yantra::ExtractorInput input { data_variant };

    auto extractor = matrix->create_operation<Yantra::UniversalExtractor>("fft");

    extractor->set_parameter("method", "fft");

    auto result = extractor->apply_operation(input);

    if (auto fft_result = std::get_if<std::vector<std::complex<double>>>(&result.base_output)) {
        return *fft_result;
    }

    return {};
}

std::vector<double> get_spectrum(const std::vector<double>& data)
{
    auto fft_result = get_fft(data);
    std::vector<double> spectrum;
    spectrum.reserve(fft_result.size());

    for (const auto& bin : fft_result) {
        spectrum.push_back(std::abs(bin));
    }

    return spectrum;
}

std::vector<double> get_power_spectrum(const std::vector<double>& data)
{
    auto fft_result = get_fft(data);
    std::vector<double> power_spectrum;
    power_spectrum.reserve(fft_result.size());

    for (const auto& bin : fft_result) {
        double magnitude = std::abs(bin);
        power_spectrum.push_back(magnitude * magnitude);
    }

    return power_spectrum;
}

SpectralFeatures analyze_spectrum(const std::vector<double>& audio_data)
{
    AnalysisParams default_params;
    return analyze_spectrum(audio_data, default_params);
}

SpectralFeatures analyze_spectrum(const std::vector<double>& data, const AnalysisParams& params)
{
    SpectralFeatures features;

    if (data.empty())
        return features;

    auto matrix = get_default_compute_matrix();
    Yantra::AnalyzerInput input = Yantra::convert_to_analyzer_input(data);
    auto analyzer = matrix->create_operation<Yantra::EnergyAnalyzer>("energy");

    analyzer->set_parameter("window_type", static_cast<int>(params.window));
    analyzer->set_parameter("window_size", static_cast<double>(params.window_size));
    analyzer->set_parameter("hop_size", static_cast<double>(params.hop_size));
    analyzer->set_parameter("normalize", params.normalize);
    analyzer->set_parameter("zero_pad", params.zero_pad);

    analyzer->set_parameter("method", "spectral_centroid");
    auto centroid_result = analyzer->apply_operation(input);
    if (auto values = std::get_if<std::vector<double>>(&centroid_result)) {
        if (!values->empty())
            features.centroid = (*values)[0];
    }

    analyzer->set_parameter("method", "spectral_rolloff");
    auto rolloff_result = analyzer->apply_operation(input);
    if (auto values = std::get_if<std::vector<double>>(&rolloff_result)) {
        if (!values->empty())
            features.rolloff = (*values)[0];
    }

    features.flux = 0.0;

    auto stat_analyzer = matrix->create_operation<Yantra::StatisticalAnalyzer>("statistical");
    auto spectrum = get_spectrum(data);

    if (!spectrum.empty()) {
        size_t high_freq_start = spectrum.size() / 2;
        std::vector<double> high_freq(spectrum.begin() + high_freq_start, spectrum.end());

        Yantra::AnalyzerInput high_freq_input = Yantra::convert_to_analyzer_input(high_freq);
        stat_analyzer->set_parameter("method", "sum");
        auto high_energy_result = stat_analyzer->apply_operation(high_freq_input);

        Yantra::AnalyzerInput spectrum_input = Yantra::convert_to_analyzer_input(spectrum);
        auto total_energy_result = stat_analyzer->apply_operation(spectrum_input);

        if (auto high_energy = std::get_if<std::vector<double>>(&high_energy_result)) {
            if (auto total_energy = std::get_if<std::vector<double>>(&total_energy_result)) {
                if (!high_energy->empty() && !total_energy->empty() && (*total_energy)[0] > 0.0) {
                    features.brightness = (*high_energy)[0] / (*total_energy)[0];
                }
            }
        }

        stat_analyzer->set_parameter("method", "variance");
        auto variance_result = stat_analyzer->apply_operation(spectrum_input);
        if (auto variance = std::get_if<std::vector<double>>(&variance_result)) {
            if (!variance->empty()) {
                features.bandwidth = std::sqrt((*variance)[0]);
            }
        }
    }

    return features;
}

//-------------------------------------------------------------------------
// Pipeline Implementation
//-------------------------------------------------------------------------

DataPipeline::DataPipeline(std::vector<double> data)
    : m_data(std::move(data))
{
}

DataPipeline DataPipeline::analyze_mean()
{
    double mean_value = get_mean(m_data);
    return DataPipeline({ mean_value });
}

DataPipeline DataPipeline::analyze_variance()
{
    double variance_value = get_variance(m_data);
    return DataPipeline({ variance_value });
}

DataPipeline DataPipeline::analyze_spectrum()
{
    auto features = MayaFlux::analyze_spectrum(m_data);
    std::vector<double> feature_values = {
        features.centroid, features.rolloff, features.flux,
        features.brightness, features.bandwidth
    };
    return DataPipeline(std::move(feature_values));
}

DataPipeline DataPipeline::sort_ascending()
{
    MayaFlux::sort_ascending(m_data);
    return DataPipeline(std::move(m_data));
}

DataPipeline DataPipeline::sort_descending()
{
    MayaFlux::sort_descending(m_data);
    return DataPipeline(std::move(m_data));
}

DataPipeline DataPipeline::extract_peaks(double threshold)
{
    auto peaks = MayaFlux::extract_peaks(m_data, threshold);
    std::vector<double> peak_values;
    peak_values.reserve(peaks.size());
    for (auto idx : peaks) {
        peak_values.push_back(static_cast<double>(idx));
    }
    return DataPipeline(std::move(peak_values));
}

DataPipeline DataPipeline::extract_envelope(size_t window_size)
{
    auto envelope = MayaFlux::extract_envelope(m_data, window_size);
    return DataPipeline(std::move(envelope));
}

DataPipeline DataPipeline::filter_above(double threshold)
{
    auto filtered = extract_above_threshold(m_data, threshold);
    return DataPipeline(std::move(filtered));
}

DataPipeline DataPipeline::filter_range(double min, double max)
{
    auto matrix = get_default_compute_matrix();

    Kakshya::DataVariant data_variant { m_data };
    Yantra::ExtractorInput input { data_variant };

    auto extractor = matrix->create_operation<Yantra::UniversalExtractor>("filter");

    extractor->set_parameter("method", "range");
    extractor->set_parameter("min_value", min);
    extractor->set_parameter("max_value", max);

    auto result = extractor->apply_operation(input);

    if (auto filtered = std::get_if<std::vector<double>>(&result.base_output)) {
        return DataPipeline(std::move(*filtered));
    }

    return DataPipeline({});
}

std::vector<double> DataPipeline::get()
{
    return std::move(m_data);
}

DataPipeline::operator std::vector<double>()
{
    return std::move(m_data);
}

DataPipeline pipeline(const std::vector<double>& data)
{
    return DataPipeline(data);
}

//-------------------------------------------------------------------------
// System Functions
//-------------------------------------------------------------------------

std::shared_ptr<Yantra::ComputeMatrix> get_compute_matrix()
{
    return get_default_compute_matrix();
}

void register_all_analysis_operations()
{
    auto matrix = get_default_compute_matrix();

    REGISTER_ANALYZER(matrix, "statistical", Yantra::StatisticalAnalyzer);
    REGISTER_ANALYZER(matrix, "energy", Yantra::EnergyAnalyzer);

    // Register built-in extractors for common operations
    // Note: These would need to be implemented as proper extractor classes
    // REGISTER_EXTRACTOR(matrix, "peaks", Yantra::PeakExtractor);
    // REGISTER_EXTRACTOR(matrix, "onsets", Yantra::OnsetExtractor);
    // REGISTER_EXTRACTOR(matrix, "envelope", Yantra::EnvelopeExtractor);
    // REGISTER_EXTRACTOR(matrix, "fft", Yantra::FFTExtractor);
    // REGISTER_EXTRACTOR(matrix, "filter", Yantra::FilterExtractor);

    // Register built-in sorters
    // REGISTER_SORTER(matrix, "universal", Yantra::UniversalSorter);
}

std::vector<std::string> get_available_analysis_methods()
{
    auto matrix = get_default_compute_matrix();

    std::vector<std::string> methods;

    auto stat_analyzer = matrix->create_operation<Yantra::StatisticalAnalyzer>("statistical");
    if (stat_analyzer) {
        auto stat_methods = stat_analyzer->get_available_methods();
        methods.insert(methods.end(), stat_methods.begin(), stat_methods.end());
    }

    auto energy_analyzer = matrix->create_operation<Yantra::EnergyAnalyzer>("energy");
    if (energy_analyzer) {
        auto energy_methods = energy_analyzer->get_available_methods();
        methods.insert(methods.end(), energy_methods.begin(), energy_methods.end());
    }

    return methods;
}

std::vector<std::string> get_available_sort_methods()
{
    auto matrix = get_default_compute_matrix();

    auto sorter = matrix->create_operation<Yantra::UniversalSorter>("universal");
    if (sorter) {
        return sorter->get_available_methods();
    }

    return {
        "ascending", "descending", "by_magnitude", "by_duration",
        "by_position", "by_energy", "indices"
    };
}

std::vector<std::string> get_available_extraction_methods()
{
    auto matrix = get_default_compute_matrix();

    std::vector<std::string> methods;

    auto extractor = matrix->create_operation<Yantra::UniversalExtractor>("peaks");
    if (extractor) {
        auto extractor_methods = extractor->get_available_methods();
        methods.insert(methods.end(), extractor_methods.begin(), extractor_methods.end());
    }

    if (methods.empty()) {
        methods = {
            "peaks", "onsets", "envelope", "above_threshold",
            "fft", "spectrum", "power_spectrum", "range"
        };
    }

    return methods;
}

}
