#pragma once

#include "MayaFlux/Yantra/ComputeMatrix.hpp"

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
}

namespace MayaFlux::Yantra {

/**
 * @class SignalAnalyzer
 * @brief Base template class for operations that compute analytical features from signal data
 *
 * Defines the interface for all analysis operations that extract quantitative
 * measurements or descriptive features from signal data. Supports optional
 * preprocessing for computationally intensive analyses.
 *
 * @tparam OutputType The type of analytical data produced by the analyzer
 */
template <typename OutputType>
class SignalAnalyzer : public ComputeOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, OutputType> {
public:
    /**
     * @brief Computes analytical features from the input signal
     * @param input Container with the signal data to analyze
     * @return Analytical features according to the specific analyzer's algorithm
     */
    virtual OutputType apply_operation(std::shared_ptr<Kakshya::SignalSourceContainer> input) override = 0;

    /**
     * @brief Indicates whether this analyzer needs preprocessing before analysis
     * @return True if preprocessing is required, false otherwise
     */
    virtual bool requires_preprocessing() const = 0;

    /**
     * @brief Performs preparatory computations on the input signal
     * @param input Container with the signal data to preprocess
     */
    virtual void preprocess(std::shared_ptr<Kakshya::SignalSourceContainer> input) = 0;
};

/**
 * @class EnergyAnalyzer
 * @brief Analyzer that computes the temporal energy distribution of a signal
 *
 * Calculates the energy content of a signal across time using windowed analysis,
 * providing a measure of signal intensity variations throughout the duration.
 */
class EnergyAnalyzer : public SignalAnalyzer<std::vector<double>> {
public:
    /**
     * @brief Constructs an energy analyzer with specified window parameters
     * @param window_size Size of the analysis window in samples
     * @param hop_size Step size between consecutive analysis windows
     */
    EnergyAnalyzer(u_int32_t window_size = 512, u_int32_t hop_size = 256);

    /**
     * @brief Computes the energy profile of the input signal
     * @param input Container with the signal data to analyze
     * @return Vector of energy values for each analysis window
     */
    virtual std::vector<double> apply_operation(std::shared_ptr<Kakshya::SignalSourceContainer> input) override;

private:
    /** @brief Size of the analysis window in samples */
    u_int32_t m_window_size;
    /** @brief Step size between consecutive analysis windows */
    u_int32_t m_hop_size;
};

/**
 * @class SpectralCentroidAnalyzer
 * @brief Analyzer that computes the spectral centroid of a signal over time
 *
 * Calculates the weighted mean frequency of the signal's spectrum for each analysis
 * frame, providing a measure of the spectral "center of mass" that correlates with
 * perceived brightness or timbral quality.
 */
class SpectralCentroidAnalyzer : public SignalAnalyzer<std::vector<double>> {
public:
    /**
     * @brief Constructs a spectral centroid analyzer with specified FFT parameters
     * @param fft_size Size of the FFT window for spectral analysis
     * @param hop_size Step size between consecutive FFT windows
     */
    SpectralCentroidAnalyzer(u_int32_t fft_size = 2048, u_int32_t hop_size = 512);

    /**
     * @brief Computes the spectral centroid profile of the input signal
     * @param input Container with the signal data to analyze
     * @return Vector of spectral centroid values for each analysis frame
     */
    virtual std::vector<double> apply_operation(std::shared_ptr<Kakshya::SignalSourceContainer> input) override;

private:
    /** @brief Size of the FFT window for spectral analysis */
    u_int32_t m_fft_size;
    /** @brief Step size between consecutive FFT windows */
    u_int32_t m_hop_size;
};
}
