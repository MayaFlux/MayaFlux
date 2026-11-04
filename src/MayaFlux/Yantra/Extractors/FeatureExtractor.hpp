#pragma once

#include "ExtractionHelper.hpp"
#include "MayaFlux/Kinesis/Discrete/Extract.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "Extractor.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

/**
 * @file FeatureExtractor.hpp
 * @brief Concrete feature extractor using typed, method-selected extraction
 *
 * FeatureExtractor extracts the data around features of a signal. The method is
 * an ExtractionMethod value and every setting lives in FeatureExtractorConfig.
 * The extracted result is carried on the output's facets as FeatureExtraction.
 *
 * Example Usage:
 * ```cpp
 * FeatureExtractorConfig config;
 * config.method = ExtractionMethod::HIGH_ENERGY_DATA;
 * config.energy_threshold = 0.2;
 * auto extractor = std::make_shared<StandardFeatureExtractor>(config);
 *
 * auto high_energy = extractor->extract_data(audio_data);
 *
 * const auto* result = extractor->apply_operation(audio_data).facets.get<FeatureExtraction>();
 * ```
 */

namespace MayaFlux::Yantra {

/**
 * @enum ExtractionMethod
 * @brief Supported extraction methods for FeatureExtractor
 */
enum ExtractionMethod : uint8_t {
    HIGH_ENERGY_DATA, ///< Extract data from high-energy regions
    PEAK_DATA, ///< Extract data around detected peaks
    OUTLIER_DATA, ///< Extract data from statistical outlier regions
    HIGH_SPECTRAL_DATA, ///< Extract data from high spectral energy regions
    ABOVE_MEAN_DATA, ///< Extract data above statistical mean
    OVERLAPPING_WINDOWS, ///< Extract overlapping windowed data
    ZERO_CROSSING_DATA, ///< Extract actual data at zero crossing points
    SILENCE_DATA, ///< Extract actual silent regions
    ONSET_DATA ///< Extract actual onset/transient regions
};

/**
 * @struct FeatureExtractorConfig
 * @brief Settings of a FeatureExtractor.
 *
 * Settings that belong to one method are prefixed with that method's name, so
 * each keeps its own default. Pass this to the constructor or to set_config.
 */
struct FeatureExtractorConfig : ExtractorConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            ExtractorConfig::describe_facets(),
            std::tuple {
                FacetMember<&FeatureExtractorConfig::window_size> {},
                FacetMember<&FeatureExtractorConfig::hop_size> {},
                FacetMember<&FeatureExtractorConfig::method> {},
                FacetMember<&FeatureExtractorConfig::energy_threshold> {},
                FacetMember<&FeatureExtractorConfig::peak_threshold> {},
                FacetMember<&FeatureExtractorConfig::peak_min_distance> {},
                FacetMember<&FeatureExtractorConfig::peak_region_size> {},
                FacetMember<&FeatureExtractorConfig::std_dev_threshold> {},
                FacetMember<&FeatureExtractorConfig::spectral_threshold> {},
                FacetMember<&FeatureExtractorConfig::mean_multiplier> {},
                FacetMember<&FeatureExtractorConfig::overlap> {},
                FacetMember<&FeatureExtractorConfig::zero_crossing_threshold> {},
                FacetMember<&FeatureExtractorConfig::zero_crossing_min_distance> {},
                FacetMember<&FeatureExtractorConfig::zero_crossing_region_size> {},
                FacetMember<&FeatureExtractorConfig::silence_threshold> {},
                FacetMember<&FeatureExtractorConfig::silence_min_duration> {},
                FacetMember<&FeatureExtractorConfig::onset_threshold> {},
                FacetMember<&FeatureExtractorConfig::onset_region_size> {},
                FacetMember<&FeatureExtractorConfig::onset_fft_window_size> {},
            });
    }

    /** Analysis window length in samples. */
    uint32_t window_size { 512 };
    /** Distance between successive windows in samples. */
    uint32_t hop_size { 256 };
    /** Feature selection method, as an ExtractionMethod value. */
    Reflect::EnumByte<ExtractionMethod> method { ExtractionMethod::HIGH_ENERGY_DATA };

    /** HIGH_ENERGY_DATA: window energy threshold. */
    double energy_threshold { 0.1 };

    /** PEAK_DATA: amplitude a peak must exceed. */
    double peak_threshold { 0.1 };
    /** PEAK_DATA: minimum distance between peaks in samples. */
    double peak_min_distance { 10.0 };
    /** PEAK_DATA: samples kept around each peak. */
    uint32_t peak_region_size { 256 };

    /** OUTLIER_DATA: standard deviations from the mean. */
    double std_dev_threshold { 2.0 };

    /** HIGH_SPECTRAL_DATA: spectral energy threshold. */
    double spectral_threshold { 0.1 };

    /** ABOVE_MEAN_DATA: multiple of the mean energy. */
    double mean_multiplier { 1.5 };

    /** OVERLAPPING_WINDOWS: overlap ratio in [0, 1). */
    double overlap { 0.5 };

    /** ZERO_CROSSING_DATA: level at which a crossing is detected. */
    double zero_crossing_threshold { 0.0 };
    /** ZERO_CROSSING_DATA: minimum distance between crossings in samples. */
    double zero_crossing_min_distance { 1.0 };
    /** ZERO_CROSSING_DATA: samples kept around each crossing. */
    uint32_t zero_crossing_region_size { 1 };

    /** SILENCE_DATA: amplitude below which a window is silent. */
    double silence_threshold { 0.01 };
    /** SILENCE_DATA: shortest silent run in samples. */
    uint32_t silence_min_duration { 1024 };

    /** ONSET_DATA: normalised spectral flux threshold. */
    double onset_threshold { 0.3 };
    /** ONSET_DATA: samples kept around each onset. */
    uint32_t onset_region_size { 512 };
    /** ONSET_DATA: FFT window length in samples. */
    uint32_t onset_fft_window_size { 1024 };
};

/**
 * @struct FeatureExtraction
 * @brief Record of one FeatureExtractor run, stored on the output's facets.
 */
struct FeatureExtraction {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&FeatureExtraction::config> {},
            FacetMember<&FeatureExtraction::extracted_samples> {},
        };
    }

    /** Settings the extraction was produced under. */
    FeatureExtractorConfig config;
    /** Total samples across all channels in the extracted output. */
    size_t extracted_samples {};
};

/**
 * @class FeatureExtractor
 * @brief Method-selected feature extractor with typed configuration
 *
 * All extraction logic is delegated to ExtractionHelper functions.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = std::vector<std::vector<double>>>
class FeatureExtractor : public Extractor<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Extractor<InputType, OutputType>;

    /**
     * @brief Construct a feature extractor from its settings.
     * @param config Extraction settings, validated before they are stored.
     */
    explicit FeatureExtractor(FeatureExtractorConfig config = {})
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    /**
     * @brief The stored settings.
     */
    [[nodiscard]] const FeatureExtractorConfig& config() const override
    {
        return *this->template resolved_config<FeatureExtractorConfig>();
    }

    /**
     * @brief Get extraction type category
     * @return ExtractionType::FEATURE_GUIDED
     */
    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::FEATURE_GUIDED;
    }

    /**
     * @brief Input validation
     */
    bool validate_extraction_input(const input_type& input) const override
    {
        try {
            if constexpr (RequiresContainer<input_type>) {
                if (!input.has_container())
                    return false;
            }
            auto numeric_data = OperationHelper::extract_numeric_data(input.data);
            if (numeric_data.empty())
                return false;
            for (const auto& span : numeric_data) {
                return Kinesis::Discrete::validate_window_parameters(
                    config().window_size, config().hop_size, span.size());
            }
            return true;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Input validation failed: {}", e.what());
            return false;
        }
    }

protected:
    [[nodiscard]] ExtractionScope extraction_scope() const override
    {
        return config().scope;
    }

    /**
     * @brief Core extraction implementation - delegates to ExtractionHelper
     * @param input Input data
     * @return Extracted data, with the FeatureExtraction record on its facets
     */
    output_type extract_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = config();

            auto [numeric_data, info] = OperationHelper::extract_structured_double(const_cast<input_type&>(input));
            DataStructureInfo structure_info = info;

            std::vector<std::span<const double>> channels;
            channels.reserve(numeric_data.size());
            for (auto& s : numeric_data)
                channels.emplace_back(s.data(), s.size());

            const uint32_t window = cfg.window_size;
            const uint32_t hop = cfg.hop_size;

            std::vector<std::vector<double>> extracted_data;

            switch (static_cast<ExtractionMethod>(cfg.method)) {

            case ExtractionMethod::HIGH_ENERGY_DATA:
                extracted_data = extract_high_energy(channels, cfg.energy_threshold, window, hop);
                break;

            case ExtractionMethod::PEAK_DATA:
                extracted_data = extract_peaks(channels, cfg.peak_threshold, cfg.peak_min_distance, cfg.peak_region_size);
                break;

            case ExtractionMethod::OUTLIER_DATA:
                extracted_data = extract_outliers(channels, cfg.std_dev_threshold, window, hop);
                break;

            case ExtractionMethod::HIGH_SPECTRAL_DATA:
                extracted_data = extract_high_spectral(channels, cfg.spectral_threshold, window, hop);
                break;

            case ExtractionMethod::ABOVE_MEAN_DATA:
                extracted_data = extract_above_mean(channels, cfg.mean_multiplier, window, hop);
                break;

            case ExtractionMethod::OVERLAPPING_WINDOWS:
                extracted_data = extract_overlapping_windows(channels, window, cfg.overlap);
                break;

            case ExtractionMethod::ZERO_CROSSING_DATA:
                extracted_data = extract_zero_crossings(channels,
                    cfg.zero_crossing_threshold,
                    cfg.zero_crossing_min_distance,
                    cfg.zero_crossing_region_size);
                break;

            case ExtractionMethod::SILENCE_DATA:
                extracted_data = extract_silence(channels,
                    cfg.silence_threshold,
                    cfg.silence_min_duration,
                    window, hop);
                break;

            case ExtractionMethod::ONSET_DATA:
                extracted_data = extract_onsets(channels,
                    cfg.onset_threshold,
                    cfg.onset_region_size,
                    cfg.onset_fft_window_size,
                    hop);
                break;

            default:
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Unknown extraction method");
            }

            output_type output = this->convert_result(extracted_data, structure_info);

            FeatureExtraction record;
            record.config = cfg;
            for (const auto& channel : extracted_data)
                record.extracted_samples += channel.size();

            output.facets = input.facets;
            output.facets.set(record);

            return output;

        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Feature extraction failed: {}", e.what());
            return output_type {};
        }
    }

private:
    static void validate_config(const FeatureExtractorConfig& cfg)
    {
        if (cfg.window_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Window size must be greater than 0");
        }
        if (cfg.hop_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Hop size must be greater than 0");
        }
        if (cfg.hop_size > cfg.window_size) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Hop size should not exceed window size for optimal coverage");
        }
    }
};

/// Standard feature extractor: vector of [DataVariant -> vector<double>]
using StandardFeatureExtractor = FeatureExtractor<std::vector<Kakshya::DataVariant>, std::vector<std::vector<double>>>;

/// Eigen Matrix feature extractor: DataVariant -> Matrixxd
using MatrixFeatureExtractor = FeatureExtractor<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container feature extractor: SignalContainer -> multi vector<double>
using ContainerFeatureExtractor = FeatureExtractor<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<std::vector<double>>>;

/// Region feature extractor: Region -> multi vector<double>
using RegionFeatureExtractor = FeatureExtractor<Kakshya::Region, std::vector<std::vector<double>>>;

/// Variant feature extractor: DataVariant -> DataVariant
using VariantFeatureExtractor = FeatureExtractor<std::vector<Kakshya::DataVariant>, std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
