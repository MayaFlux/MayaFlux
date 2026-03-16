#pragma once

#include "ExtractionHelper.hpp"
#include "MayaFlux/Kinesis/Discrete/Extract.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "UniversalExtractor.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

/**
 * @file FeatureExtractor.hpp
 * @brief Concrete feature extractor using analyzer-guided extraction
 *
 * FeatureExtractor demonstrates the modern extraction paradigm by using
 * analyzers (EnergyAnalyzer, StatisticalAnalyzer) to identify regions of interest,
 * then extracting the actual data from those regions. Uses enum-based configuration
 * instead of string parsing for type safety and performance.
 *
 * Example Usage:
 * ```cpp
 * // Extract high-energy audio data
 * auto extractor = std::make_shared<StandardFeatureExtractor>();
 * extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);
 * extractor->set_parameter("energy_threshold", 0.2);
 *
 * auto high_energy_audio = extractor->extract_data(audio_data);
 *
 * // Extract data from statistical outlier regions
 * extractor->set_extraction_method(ExtractionMethod::OUTLIER_DATA);
 * extractor->set_parameter("std_dev_threshold", 2.5);
 *
 * auto outlier_audio = extractor->extract_data(audio_data);
 * ```
 */

namespace MayaFlux::Yantra {

/**
 * @enum ExtractionMethod
 * @brief Supported extraction methods for FeatureExtractor
 */
enum class ExtractionMethod : uint8_t {
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
 * @class FeatureExtractor
 * @brief Analyzer-guided feature extractor with enum-based configuration
 *
 * Uses analyzers to identify regions of interest based on energy, statistical properties,
 * or spectral characteristics, then extracts the actual data from those regions.
 * All extraction logic is delegated to ExtractionHelper functions.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = std::vector<std::vector<double>>>
class MAYAFLUX_API FeatureExtractor : public UniversalExtractor<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = UniversalExtractor<InputType, OutputType>;

    /**
     * @brief Construct FeatureExtractor with default parameters
     * @param window_size Analysis window size (default: 512)
     * @param hop_size Hop size between windows (default: 256)
     * @param method Initial extraction method (default: HIGH_ENERGY_DATA)
     */
    explicit FeatureExtractor(uint32_t window_size = 512,
        uint32_t hop_size = 256,
        ExtractionMethod method = ExtractionMethod::HIGH_ENERGY_DATA)
        : m_window_size(window_size)
        , m_hop_size(hop_size)
        , m_method(method)
    {
        validate_parameters();
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
     * @brief Get available extraction methods
     * @return Vector of supported method names
     */
    [[nodiscard]] std::vector<std::string> get_available_methods() const override
    {
        return Reflect::get_enum_names_lowercase<ExtractionMethod>();
    }

    /**
     * @brief Set extraction method using enum
     * @param method ExtractionMethod enum value
     */
    void set_extraction_method(ExtractionMethod method)
    {
        m_method = method;
        this->set_parameter("method", method_to_string(method));
    }

    /**
     * @brief Set extraction method using string (case-insensitive)
     * @param method_name String representation of method
     */
    void set_extraction_method(const std::string& method_name)
    {
        m_method = string_to_method(method_name);
        this->set_parameter("method", method_name);
    }

    /**
     * @brief Get current extraction method
     * @return ExtractionMethod enum value
     */
    [[nodiscard]] ExtractionMethod get_extraction_method() const
    {
        return m_method;
    }

    /**
     * @brief Set window size for analysis
     * @param size Window size in samples
     */
    void set_window_size(uint32_t size)
    {
        m_window_size = size;
        validate_parameters();
    }

    /**
     * @brief Set hop size for analysis
     * @param size Hop size in samples
     */
    void set_hop_size(uint32_t size)
    {
        m_hop_size = size;
        validate_parameters();
    }

    /**
     * @brief Get current window size
     * @return Window size in samples
     */
    [[nodiscard]] uint32_t get_window_size() const { return m_window_size; }

    /**
     * @brief Get current hop size
     * @return Hop size in samples
     */
    [[nodiscard]] uint32_t get_hop_size() const { return m_hop_size; }

    /**
     * @brief Convert extraction method enum to string
     * @param method ExtractionMethod value
     * @return Lowercase string representation
     */
    static std::string method_to_string(ExtractionMethod method)
    {
        return Reflect::enum_to_lowercase_string(method);
    }

    /**
     * @brief Convert string to extraction method enum
     * @param str String representation (case-insensitive)
     * @return ExtractionMethod value
     */
    static ExtractionMethod string_to_method(const std::string& str)
    {
        return Reflect::string_to_enum_or_throw_case_insensitive<ExtractionMethod>(str, "ExtractionMethod");
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
                return Kinesis::Discrete::validate_window_parameters(m_window_size, m_hop_size, span.size());
            }
            return true;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Input validation failed: {}", e.what());
            return false;
        }
    }

    /**
     * @brief Get extractor name
     * @return "FeatureExtractor"
     */
    [[nodiscard]] std::string get_extractor_name() const override
    {
        return "FeatureExtractor";
    }

protected:
    /**
     * @brief Core extraction implementation - delegates to ExtractionHelper
     * @param input Input data with metadata
     * @return Extracted data with metadata
     */
    output_type extract_implementation(const input_type& input) override
    {
        try {
            auto [numeric_data, info] = OperationHelper::extract_structured_double(const_cast<input_type&>(input));
            DataStructureInfo structure_info = info;

            std::vector<std::span<const double>> channels;
            channels.reserve(numeric_data.size());
            for (auto& s : numeric_data)
                channels.emplace_back(s.data(), s.size());

            std::vector<std::vector<double>> extracted_data;

            switch (m_method) {

            case ExtractionMethod::HIGH_ENERGY_DATA:
                extracted_data = extract_high_energy(channels,
                    this->template get_parameter_or_default<double>("energy_threshold", 0.1),
                    m_window_size, m_hop_size);
                break;

            case ExtractionMethod::PEAK_DATA:
                extracted_data = extract_peaks(channels,
                    this->template get_parameter_or_default<double>("threshold", 0.1),
                    this->template get_parameter_or_default<double>("min_distance", 10.0),
                    this->template get_parameter_or_default<uint32_t>("region_size", 256));
                break;

            case ExtractionMethod::OUTLIER_DATA:
                extracted_data = extract_outliers(channels,
                    this->template get_parameter_or_default<double>("std_dev_threshold", 2.0),
                    m_window_size, m_hop_size);
                break;

            case ExtractionMethod::HIGH_SPECTRAL_DATA:
                extracted_data = extract_high_spectral(channels,
                    this->template get_parameter_or_default<double>("spectral_threshold", 0.1),
                    m_window_size, m_hop_size);
                break;

            case ExtractionMethod::ABOVE_MEAN_DATA:
                extracted_data = extract_above_mean(channels,
                    this->template get_parameter_or_default<double>("mean_multiplier", 1.5),
                    m_window_size, m_hop_size);
                break;

            case ExtractionMethod::OVERLAPPING_WINDOWS:
                extracted_data = extract_overlapping_windows(channels,
                    m_window_size,
                    this->template get_parameter_or_default<double>("overlap", 0.5));
                break;

            case ExtractionMethod::ZERO_CROSSING_DATA:
                extracted_data = extract_zero_crossings(channels,
                    this->template get_parameter_or_default<double>("threshold", 0.0),
                    this->template get_parameter_or_default<double>("min_distance", 1.0),
                    this->template get_parameter_or_default<uint32_t>("region_size", 1));
                break;

            case ExtractionMethod::SILENCE_DATA:
                extracted_data = extract_silence(channels,
                    this->template get_parameter_or_default<double>("silence_threshold", 0.01),
                    this->template get_parameter_or_default<uint32_t>("min_duration", 1024),
                    m_window_size, m_hop_size);
                break;

            case ExtractionMethod::ONSET_DATA:
                extracted_data = extract_onsets(channels,
                    this->template get_parameter_or_default<double>("threshold", 0.3),
                    this->template get_parameter_or_default<uint32_t>("region_size", 512),
                    this->template get_parameter_or_default<uint32_t>("fft_window_size", 1024),
                    m_hop_size);
                break;

            default:
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Unknown extraction method");
            }

            output_type output = this->convert_result(extracted_data, structure_info);

            output.template set_metadata<std::string>("extractor_type", "FeatureExtractor");
            output.template set_metadata<std::string>("extraction_method", method_to_string(m_method));
            output.template set_metadata<uint32_t>("window_size", static_cast<uint32_t>(m_window_size));
            output.template set_metadata<uint32_t>("hop_size", static_cast<uint32_t>(m_hop_size));
            output.template set_metadata<size_t>("extracted_samples", extracted_data.size());

            return output;

        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Feature extraction failed: {}", e.what());
            return output_type {};
        }
    }

    /**
     * @brief Handle extractor-specific parameters
     */
    void set_extraction_parameter(const std::string& name, std::any value) override
    {
        if (name == "method") {
            if (auto result = safe_any_cast<std::string>(value)) {
                m_method = string_to_method(*result.value);
                return;
            }
            if (auto result = safe_any_cast<ExtractionMethod>(value)) {
                m_method = *result.value;
                return;
            }
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Method parameter must be string or ExtractionMethod enum");
        }

        if (name == "window_size") {
            if (auto result = safe_any_cast<uint32_t>(value)) {
                m_window_size = *result.value;
                validate_parameters();
                return;
            }
        }
        if (name == "hop_size") {
            if (auto result = safe_any_cast<uint32_t>(value)) {
                m_hop_size = *result.value;
                validate_parameters();
                return;
            }
        }

        base_type::set_extraction_parameter(name, std::move(value));
    }

    [[nodiscard]] std::any get_extraction_parameter(const std::string& name) const override
    {
        if (name == "method") {
            return method_to_string(m_method);
        }
        if (name == "window_size") {
            return m_window_size;
        }
        if (name == "hop_size") {
            return m_hop_size;
        }

        return base_type::get_extraction_parameter(name);
    }

private:
    uint32_t m_window_size;
    uint32_t m_hop_size;
    ExtractionMethod m_method;

    /**
     * @brief Validate extraction parameters
     */
    void validate_parameters() const
    {
        if (m_window_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Window size must be greater than 0");
        }
        if (m_hop_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Hop size must be greater than 0");
        }
        if (m_hop_size > m_window_size) {
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
