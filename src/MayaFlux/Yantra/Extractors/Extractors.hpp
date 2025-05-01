#pragma once

#include "MayaFlux/Yantra/ComputeMatrix.hpp"

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
struct RegionSegment;
}

namespace MayaFlux::Yantra {

/**
 * @class RegionExtractor
 * @brief Base template class for operations that identify and extract regions of interest from signal data
 *
 * Defines the interface for all extraction operations that analyze signal data
 * to identify meaningful segments or features based on various criteria.
 *
 * @tparam OutputType The type of data produced by the extraction operation
 */
template <typename OutputType>
class RegionExtractor : public ComputeOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, OutputType> {
public:
    /**
     * @brief Analyzes the input signal and extracts regions of interest
     * @param input Container with the signal data to analyze
     * @return Extracted data according to the specific extractor's criteria
     */
    virtual OutputType apply_operation(std::shared_ptr<Kakshya::SignalSourceContainer> input) override = 0;
};

/**
 * @class SpectralDensityExtractor
 * @brief Extractor that identifies regions based on spectral energy distribution
 *
 * Analyzes the frequency domain representation of a signal to identify
 * regions where the spectral energy exceeds a specified threshold,
 * indicating areas of significant frequency content.
 */
class SpectralDensityExtractor : public RegionExtractor<std::vector<Kakshya::RegionSegment>> {
public:
    /**
     * @brief Constructs a spectral density-based region extractor
     * @param threshold Energy threshold for region detection (0.0-1.0)
     * @param fft_size Size of the FFT window for spectral analysis
     * @param hop_size Step size between consecutive FFT windows
     */
    SpectralDensityExtractor(double threshold = 0.5, u_int32_t fft_size = 2048, u_int32_t hop_size = 512);

    /**
     * @brief Extracts regions with significant spectral density
     * @param input Container with the signal data to analyze
     * @return Vector of region segments where spectral density exceeds the threshold
     */
    virtual std::vector<Kakshya::RegionSegment> apply_operation(std::shared_ptr<Kakshya::SignalSourceContainer> input) override;

private:
    /** @brief Energy threshold for region detection */
    double m_threshold;
    /** @brief Size of the FFT window for spectral analysis */
    uint32_t m_fft_size;
    /** @brief Step size between consecutive FFT windows */
    uint32_t m_hop_size;
};

/**
 * @class ZeroCrossingExtractor
 * @brief Extractor that identifies regions based on signal zero-crossing rates
 *
 * Analyzes the time-domain representation of a signal to identify
 * regions where the zero-crossing rate falls within a specified range,
 * which can indicate areas with specific frequency characteristics.
 */
class ZeroCrossingExtractor : public RegionExtractor<std::vector<Kakshya::RegionSegment>> {
public:
    /**
     * @brief Constructs a zero-crossing rate-based region extractor
     * @param min_rate Minimum zero-crossing rate for region detection
     * @param max_rate Maximum zero-crossing rate for region detection
     */
    ZeroCrossingExtractor(double min_rate = 0.01, double max_rate = 0.2);

    /**
     * @brief Extracts regions with zero-crossing rates in the specified range
     * @param input Container with the signal data to analyze
     * @return Vector of region segments with matching zero-crossing characteristics
     */
    virtual std::vector<Kakshya::RegionSegment> apply_operation(
        std::shared_ptr<Kakshya::SignalSourceContainer> input) override;

private:
    /** @brief Minimum zero-crossing rate for region detection */
    double m_min_rate;
    /** @brief Maximum zero-crossing rate for region detection */
    double m_max_rate;
};
}
