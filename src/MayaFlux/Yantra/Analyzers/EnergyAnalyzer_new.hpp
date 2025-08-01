#pragma once

#include "MayaFlux/EnumUtils.hpp"
#include "UniversalAnalyzer_new.hpp"
#include <Eigen/Dense>
#include <execution>
#include <numeric>

/**
 * @file EnergyAnalyzer_new.hpp
 * @brief Concept-based energy analysis for digital signals in Maya Flux
 *
 * Defines the EnergyAnalyzer using the new UniversalAnalyzer framework with
 * instance-defined I/O types. This analyzer extracts energy-related features
 * from digital signals with multiple computation methods and flexible output
 * configurations.
 *
 * Key Features:
 * - **Template-flexible I/O:** Instance defines input/output types at construction
 * - **Multiple energy methods:** RMS, peak, spectral, zero-crossing, harmonic, power, dynamic range
 * - **Parallel processing:** Utilizes std::execution for performance
 * - **Energy classification:** Maps values to qualitative levels (silent, quiet, etc.)
 * - **Window-based analysis:** Configurable window size and hop size
 * - **Concept-constrained:** Type safety through ComputeData concept
 */

namespace MayaFlux::Yantra {

/**
 * @enum EnergyMethod
 * @brief Supported energy computation methods
 */
enum class EnergyMethod : uint8_t {
    RMS, ///< Root Mean Square energy
    PEAK, ///< Peak amplitude
    SPECTRAL, ///< Spectral energy (FFT-based)
    ZERO_CROSSING, ///< Zero-crossing rate
    HARMONIC, ///< Harmonic energy (low-frequency content)
    POWER, ///< Power (sum of squares)
    DYNAMIC_RANGE ///< Dynamic range (dB)
};

/**
 * @enum EnergyLevel
 * @brief Qualitative classification of energy values
 */
enum class EnergyLevel : uint8_t {
    SILENT,
    QUIET,
    MODERATE,
    LOUD,
    PEAK
};

/**
 * @class EnergyAnalyzer
 * @brief Template-flexible energy analyzer with instance-defined I/O types
 *
 * The EnergyAnalyzer provides comprehensive energy analysis capabilities for
 * digital signals. Unlike the old variant-based approach, this analyzer uses
 * template parameters to define I/O types at instantiation time, providing
 * maximum flexibility while maintaining type safety.
 *
 * Example usage:
 * ```cpp
 * // DataVariant -> VectorXd analyzer
 * auto energy_analyzer = std::make_shared<EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>();
 *
 * // Region -> RegionGroup analyzer
 * auto region_energy = std::make_shared<EnergyAnalyzer<Kakshya::Region, Kakshya::RegionGroup>>();
 *
 * // SignalContainer -> DataVariant analyzer
 * auto container_energy = std::make_shared<EnergyAnalyzer<
 *     std::shared_ptr<Kakshya::SignalSourceContainer>,
 *     Kakshya::DataVariant
 * >>();
 * ```
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = Eigen::VectorXd>
class EnergyAnalyzer : public UniversalAnalyzer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = UniversalAnalyzer<InputType, OutputType>;

    /**
     * @brief Construct EnergyAnalyzer with configurable window parameters
     * @param window_size Size of analysis window in samples (default: 512)
     * @param hop_size Step size between windows in samples (default: 256)
     */
    explicit EnergyAnalyzer(uint32_t window_size = 512, uint32_t hop_size = 256)
        : m_window_size(window_size)
        , m_hop_size(hop_size)
        , m_method(EnergyMethod::RMS)
        , m_classification_enabled(false)
        , m_silent_threshold(0.01)
        , m_quiet_threshold(0.1)
        , m_moderate_threshold(0.5)
        , m_loud_threshold(0.8)
    {
        validate_window_parameters();
    }

    /**
     * @brief Get analysis type category
     * @return AnalysisType::FEATURE (energy is a feature extraction operation)
     */
    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::FEATURE;
    }

    /**
     * @brief Get available analysis methods
     * @return Vector of supported energy method names
     */
    std::vector<std::string> get_available_methods() const override
    {
        return Utils::get_enum_names_lowercase<EnergyMethod>();
    }

    /**
     * @brief Check if a specific method is supported
     * @param method Method name to check
     * @return True if method is supported
     */
    bool supports_method(const std::string& method) const override
    {
        return std::find_if(get_available_methods().begin(), get_available_methods().end(),
                   [&method](const std::string& m) {
                       return std::equal(method.begin(), method.end(), m.begin(), m.end(),
                           [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                   })
            != get_available_methods().end();
    }

    // === Configuration Methods ===

    /**
     * @brief Set energy computation method
     * @param method Energy computation method
     */
    void set_energy_method(EnergyMethod method)
    {
        m_method = method;
    }

    /**
     * @brief Get current energy computation method
     * @return Current energy method
     */
    EnergyMethod get_energy_method() const
    {
        return m_method;
    }

    /**
     * @brief Set window parameters for analysis
     * @param window_size Window size in samples
     * @param hop_size Hop size in samples
     */
    void set_window_parameters(uint32_t window_size, uint32_t hop_size)
    {
        m_window_size = window_size;
        m_hop_size = hop_size;
        validate_window_parameters();
    }

    /**
     * @brief Set energy level classification thresholds
     * @param silent Threshold for silent level
     * @param quiet Threshold for quiet level
     * @param moderate Threshold for moderate level
     * @param loud Threshold for loud level
     */
    void set_energy_thresholds(double silent, double quiet, double moderate, double loud)
    {
        if (!(silent < quiet && quiet < moderate && moderate < loud)) {
            throw std::invalid_argument("Energy thresholds must be in ascending order");
        }

        m_silent_threshold = silent;
        m_quiet_threshold = quiet;
        m_moderate_threshold = moderate;
        m_loud_threshold = loud;
    }

    /**
     * @brief Enable or disable energy level classification
     * @param enabled True to enable classification
     */
    void enable_classification(bool enabled)
    {
        m_classification_enabled = enabled;
    }

    /**
     * @brief Classify energy value into qualitative level
     * @param energy Energy value to classify
     * @return EnergyLevel classification
     */
    EnergyLevel classify_energy_level(double energy) const
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

    // === Static Utility Methods ===

    /**
     * @brief Convert energy method enum to string
     * @param method EnergyMethod value
     * @return String representation
     */
    static std::string method_to_string(EnergyMethod method)
    {
        return Utils::enum_to_lowercase_string(method);
    }

    /**
     * @brief Convert string to energy method enum
     * @param str String representation
     * @return EnergyMethod value
     */
    static EnergyMethod string_to_method(const std::string& str)
    {
        if (str == "default")
            return EnergyMethod::RMS;
        return Utils::string_to_enum_or_throw_case_insensitive<EnergyMethod>(str, "EnergyMethod");
    }

    /**
     * @brief Convert energy level enum to string
     * @param level EnergyLevel value
     * @return String representation
     */
    static std::string energy_level_to_string(EnergyLevel level)
    {
        return Utils::enum_to_lowercase_string(level);
    }

protected:
    /**
     * @brief Get analyzer name
     * @return "EnergyAnalyzer"
     */
    std::string get_analyzer_name() const override
    {
        return "EnergyAnalyzer";
    }

    /**
     * @brief Core energy analysis implementation
     * @param input Input data wrapped in IO container
     * @return Analysis results wrapped in IO container
     */
    output_type analyze_implementation(const input_type& input) override
    {
        // Extract numeric data from input
        std::vector<double> data = extract_numeric_data(input.data);

        // Compute energy values using current method
        std::vector<double> energy_values = compute_energy_values(data, m_method);

        // Convert to output type and wrap in IO
        OutputType result = convert_to_output_type(energy_values);
        output_type output(result);

        // Add metadata
        add_energy_metadata(output, energy_values);

        return output;
    }

    /**
     * @brief Handle analysis-specific parameters
     * @param name Parameter name
     * @param value Parameter value
     */
    void set_analysis_parameter(const std::string& name, std::any value) override
    {
        if (name == "method") {
            if (auto* method_str = std::any_cast<std::string>(&value)) {
                m_method = string_to_method(*method_str);
                return;
            }
            if (auto* method_enum = std::any_cast<EnergyMethod>(&value)) {
                m_method = *method_enum;
                return;
            }
        } else if (name == "window_size") {
            if (auto* size = std::any_cast<uint32_t>(&value)) {
                m_window_size = *size;
                validate_window_parameters();
                return;
            }
        } else if (name == "hop_size") {
            if (auto* size = std::any_cast<uint32_t>(&value)) {
                m_hop_size = *size;
                validate_window_parameters();
                return;
            }
        } else if (name == "classification_enabled") {
            if (auto* enabled = std::any_cast<bool>(&value)) {
                m_classification_enabled = *enabled;
                return;
            }
        }

        // Delegate to base class for unhandled parameters
        base_type::set_analysis_parameter(name, std::move(value));
    }

    /**
     * @brief Get analysis-specific parameter
     * @param name Parameter name
     * @return Parameter value
     */
    std::any get_analysis_parameter(const std::string& name) const override
    {
        if (name == "method")
            return std::any(method_to_string(m_method));
        if (name == "window_size")
            return std::any(m_window_size);
        if (name == "hop_size")
            return std::any(m_hop_size);
        if (name == "classification_enabled")
            return std::any(m_classification_enabled);

        return base_type::get_analysis_parameter(name);
    }

private:
    // === Configuration Members ===
    uint32_t m_window_size;
    uint32_t m_hop_size;
    EnergyMethod m_method;
    bool m_classification_enabled;

    // Energy level thresholds
    double m_silent_threshold;
    double m_quiet_threshold;
    double m_moderate_threshold;
    double m_loud_threshold;

    // === Helper Methods ===

    /**
     * @brief Validate window parameters
     */
    void validate_window_parameters() const
    {
        if (m_window_size == 0) {
            throw std::invalid_argument("Window size must be greater than 0");
        }
        if (m_hop_size == 0) {
            throw std::invalid_argument("Hop size must be greater than 0");
        }
        if (m_hop_size > m_window_size) {
            throw std::invalid_argument("Hop size should not exceed window size");
        }
    }

    /**
     * @brief Extract numeric data from various input types
     * @param input Input data of any ComputeData type
     * @return Vector of double values for processing
     */
    std::vector<double> extract_numeric_data(const InputType& input) const
    {
        if constexpr (std::is_same_v<InputType, Kakshya::DataVariant>) {
            return extract_from_data_variant(input);
        } else if constexpr (std::is_same_v<InputType, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            return extract_from_container(input);
        } else if constexpr (std::is_same_v<InputType, Kakshya::Region>) {
            return extract_from_region(input);
        } else if constexpr (std::is_same_v<InputType, Kakshya::RegionGroup>) {
            return extract_from_region_group(input);
        } else if constexpr (std::is_same_v<InputType, std::vector<Kakshya::RegionSegment>>) {
            return extract_from_segments(input);
        } else if constexpr (std::is_base_of_v<Eigen::MatrixBase<InputType>, InputType>) {
            return extract_from_eigen(input);
        } else {
            throw std::runtime_error("Unsupported input type for energy analysis");
        }
    }

    /**
     * @brief Extract data from DataVariant
     */
    std::vector<double> extract_from_data_variant(const Kakshya::DataVariant& data) const
    {
        // Implementation depends on your DataVariant structure
        // This is placeholder - adapt to your actual DataVariant implementation
        throw std::runtime_error("DataVariant extraction not yet implemented");
    }

    /**
     * @brief Extract data from SignalSourceContainer
     */
    std::vector<double> extract_from_container(const std::shared_ptr<Kakshya::SignalSourceContainer>& container) const
    {
        if (!container) {
            throw std::invalid_argument("Null container provided");
        }
        // Implementation depends on your container structure
        throw std::runtime_error("Container extraction not yet implemented");
    }

    /**
     * @brief Extract data from Region
     */
    std::vector<double> extract_from_region(const Kakshya::Region& region) const
    {
        // Implementation depends on your Region structure
        throw std::runtime_error("Region extraction not yet implemented");
    }

    /**
     * @brief Extract data from RegionGroup
     */
    std::vector<double> extract_from_region_group(const Kakshya::RegionGroup& group) const
    {
        // Implementation depends on your RegionGroup structure
        throw std::runtime_error("RegionGroup extraction not yet implemented");
    }

    /**
     * @brief Extract data from RegionSegments
     */
    std::vector<double> extract_from_segments(const std::vector<Kakshya::RegionSegment>& segments) const
    {
        // Implementation depends on your RegionSegment structure
        throw std::runtime_error("Segments extraction not yet implemented");
    }

    /**
     * @brief Extract data from Eigen matrix/vector
     */
    template <typename EigenType>
    std::vector<double> extract_from_eigen(const EigenType& eigen_data) const
    {
        std::vector<double> result;
        if constexpr (EigenType::IsVectorAtCompileTime) {
            result.resize(eigen_data.size());
            for (int i = 0; i < eigen_data.size(); ++i) {
                result[i] = static_cast<double>(eigen_data(i));
            }
        } else {
            // For matrices, flatten row-wise
            result.resize(eigen_data.size());
            int idx = 0;
            for (int i = 0; i < eigen_data.rows(); ++i) {
                for (int j = 0; j < eigen_data.cols(); ++j) {
                    result[idx++] = static_cast<double>(eigen_data(i, j));
                }
            }
        }
        return result;
    }

    /**
     * @brief Compute energy values using specified method
     * @param data Input signal data
     * @param method Energy computation method
     * @return Vector of energy values
     */
    std::vector<double> compute_energy_values(const std::vector<double>& data, EnergyMethod method) const
    {
        switch (method) {
        case EnergyMethod::RMS:
            return compute_rms_energy(data);
        case EnergyMethod::PEAK:
            return compute_peak_energy(data);
        case EnergyMethod::SPECTRAL:
            return compute_spectral_energy(data);
        case EnergyMethod::ZERO_CROSSING:
            return compute_zero_crossing_energy(data);
        case EnergyMethod::HARMONIC:
            return compute_harmonic_energy(data);
        case EnergyMethod::POWER:
            return compute_power_energy(data);
        case EnergyMethod::DYNAMIC_RANGE:
            return compute_dynamic_range_energy(data);
        default:
            throw std::runtime_error("Unknown energy method");
        }
    }

    /**
     * @brief Compute RMS energy over sliding windows
     */
    std::vector<double> compute_rms_energy(const std::vector<double>& data) const
    {
        if (data.size() < m_window_size) {
            throw std::invalid_argument("Data size must be at least window size");
        }

        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> rms_values(num_windows);

        // Use parallel execution for performance
        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                double sum_squares = 0.0;
                for (size_t j = start_idx; j < end_idx; ++j) {
                    sum_squares += data[j] * data[j];
                }

                rms_values[i] = std::sqrt(sum_squares / (end_idx - start_idx));
            });

        return rms_values;
    }

    /**
     * @brief Compute peak energy over sliding windows
     */
    std::vector<double> compute_peak_energy(const std::vector<double>& data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> peak_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                double max_val = 0.0;
                for (size_t j = start_idx; j < end_idx; ++j) {
                    max_val = std::max(max_val, std::abs(data[j]));
                }

                peak_values[i] = max_val;
            });

        return peak_values;
    }

    /**
     * @brief Compute spectral energy (placeholder - requires FFT)
     */
    std::vector<double> compute_spectral_energy(const std::vector<double>& data) const
    {
        // Placeholder implementation - would require FFT library
        // For now, return RMS as approximation
        return compute_rms_energy(data);
    }

    /**
     * @brief Compute zero-crossing rate
     */
    std::vector<double> compute_zero_crossing_energy(const std::vector<double>& data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> zcr_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                int zero_crossings = 0;
                for (size_t j = start_idx + 1; j < end_idx; ++j) {
                    if ((data[j] >= 0.0) != (data[j - 1] >= 0.0)) {
                        zero_crossings++;
                    }
                }

                zcr_values[i] = static_cast<double>(zero_crossings) / (end_idx - start_idx - 1);
            });

        return zcr_values;
    }

    /**
     * @brief Compute harmonic energy (placeholder)
     */
    std::vector<double> compute_harmonic_energy(const std::vector<double>& data) const
    {
        // Placeholder - would require frequency domain analysis
        return compute_rms_energy(data);
    }

    /**
     * @brief Compute power energy
     */
    std::vector<double> compute_power_energy(const std::vector<double>& data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> power_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                double sum_squares = 0.0;
                for (size_t j = start_idx; j < end_idx; ++j) {
                    sum_squares += data[j] * data[j];
                }

                power_values[i] = sum_squares;
            });

        return power_values;
    }

    /**
     * @brief Compute dynamic range energy
     */
    std::vector<double> compute_dynamic_range_energy(const std::vector<double>& data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> dr_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                double min_val = std::numeric_limits<double>::max();
                double max_val = std::numeric_limits<double>::lowest();

                for (size_t j = start_idx; j < end_idx; ++j) {
                    min_val = std::min(min_val, std::abs(data[j]));
                    max_val = std::max(max_val, std::abs(data[j]));
                }

                // Convert to dB, avoid log(0)
                if (min_val < 1e-10)
                    min_val = 1e-10;
                dr_values[i] = 20.0 * std::log10(max_val / min_val);
            });

        return dr_values;
    }

    /**
     * @brief Calculate number of windows for given data size
     */
    size_t calculate_num_windows(size_t data_size) const
    {
        if (data_size < m_window_size)
            return 0;
        return (data_size - m_window_size) / m_hop_size + 1;
    }

    /**
     * @brief Convert energy values to output type
     */
    OutputType convert_to_output_type(const std::vector<double>& energy_values) const
    {
        if constexpr (std::is_same_v<OutputType, std::vector<double>>) {
            return energy_values;
        } else if constexpr (std::is_same_v<OutputType, Eigen::VectorXd>) {
            Eigen::VectorXd result(energy_values.size());
            for (size_t i = 0; i < energy_values.size(); ++i) {
                result(i) = energy_values[i];
            }
            return result;
        } else if constexpr (std::is_same_v<OutputType, Kakshya::DataVariant>) {
            // Convert to DataVariant - implementation depends on your DataVariant
            throw std::runtime_error("DataVariant output conversion not yet implemented");
        } else {
            throw std::runtime_error("Unsupported output type for energy analysis");
        }
    }

    /**
     * @brief Add energy-specific metadata to output
     */
    void add_energy_metadata(output_type& output, const std::vector<double>& energy_values) const
    {
        output.metadata["energy_method"] = method_to_string(m_method);
        output.metadata["window_size"] = m_window_size;
        output.metadata["hop_size"] = m_hop_size;
        output.metadata["num_windows"] = energy_values.size();

        if (!energy_values.empty()) {
            output.metadata["min_energy"] = *std::min_element(energy_values.begin(), energy_values.end());
            output.metadata["max_energy"] = *std::max_element(energy_values.begin(), energy_values.end());

            double mean_energy = std::accumulate(energy_values.begin(), energy_values.end(), 0.0) / energy_values.size();
            output.metadata["mean_energy"] = mean_energy;
        }

        if (m_classification_enabled) {
            output.metadata["classification_enabled"] = true;
            output.metadata["silent_threshold"] = m_silent_threshold;
            output.metadata["quiet_threshold"] = m_quiet_threshold;
            output.metadata["moderate_threshold"] = m_moderate_threshold;
            output.metadata["loud_threshold"] = m_loud_threshold;
        }
    }
};

// === Convenient Type Aliases ===

/// Energy analyzer for DataVariant input producing VectorXd output
using StandardEnergyAnalyzer = EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>;

/// Energy analyzer for signal containers
using ContainerEnergyAnalyzer = EnergyAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::VectorXd>;

/// Energy analyzer for regions
using RegionEnergyAnalyzer = EnergyAnalyzer<Kakshya::Region, Eigen::VectorXd>;

/// Energy analyzer producing raw double vectors
template <ComputeData InputType = Kakshya::DataVariant>
using RawEnergyAnalyzer = EnergyAnalyzer<InputType, std::vector<double>>;

} // namespace MayaFlux::Yantra
