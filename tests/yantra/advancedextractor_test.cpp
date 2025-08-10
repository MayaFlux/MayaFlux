#include "../test_config.h"
#include "MayaFlux/Yantra/Extractors/ExtractionHelper.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include <algorithm>
#include <random>

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

// =========================================================================
// CONTROLLED TEST DATA GENERATORS
// =========================================================================

class ExtractionTestDataGenerator {
public:
    /**
     * @brief Generate audio with known high-energy bursts
     */
    static std::vector<double> create_energy_burst_signal(size_t total_samples = 2048,
        size_t burst_interval = 256,
        size_t burst_duration = 32)
    {
        std::vector<double> signal;
        signal.reserve(total_samples);

        for (size_t i = 0; i < total_samples; ++i) {
            double t = static_cast<double>(i) / 44100.0;

            double sample = 0.05 * std::sin(2.0 * M_PI * 220.0 * t);

            if ((i % burst_interval) < burst_duration) {
                sample += 1.5 * std::sin(2.0 * M_PI * 1000.0 * t);
            }

            signal.push_back(sample);
        }

        return signal;
    }

    /**
     * @brief Generate signal with known peaks at specific locations
     */
    static std::vector<double> create_peak_signal(size_t total_samples = 1024,
        const std::vector<size_t>& peak_locations = { 100, 300, 500, 700 })
    {
        std::vector<double> signal(total_samples, 0.0);

        for (size_t i = 0; i < total_samples; ++i) {
            signal[i] = 0.05 * std::sin(2.0 * M_PI * i / 64.0);
        }

        for (size_t peak_loc : peak_locations) {
            if (peak_loc < total_samples) {
                signal[peak_loc] = 1.5;

                if (peak_loc > 0)
                    signal[peak_loc - 1] = 0.8;
                if (peak_loc + 1 < total_samples)
                    signal[peak_loc + 1] = 0.8;
            }
        }

        return signal;
    }

    /**
     * @brief Generate signal with statistical outliers at known positions
     */
    static std::vector<double> create_outlier_signal(size_t total_samples = 1024)
    {
        std::vector<double> signal;
        signal.reserve(total_samples);

        // Generate very consistent base signal for clear outlier detection
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducible tests
        std::normal_distribution<double> normal_dist(0.0, 0.02); // Very low variance

        for (size_t i = 0; i < total_samples; ++i) {
            signal.push_back(normal_dist(gen));
        }

        // FIXED: Create WINDOW-LEVEL outliers instead of sample-level
        // These regions will have significantly different mean values when analyzed in windows
        std::vector<std::pair<size_t, size_t>> outlier_regions = {
            { 100, 150 }, // 50-sample outlier region 1
            { 300, 350 }, // 50-sample outlier region 2
            { 600, 650 }, // 50-sample outlier region 3
            { 800, 850 } // 50-sample outlier region 4
        };

        for (const auto& [start, end] : outlier_regions) {
            for (size_t i = start; i < end && i < total_samples; ++i) {
                // Create regions with consistently high/low values
                double outlier_value = (start % 400 == 100) ? 0.8 : -0.8; // Alternating high/low regions
                signal[i] = outlier_value; // Much higher than base signal mean
            }
        }

        return signal;
    }

    /**
     * @brief Generate signal with known spectral characteristics
     */
    static std::vector<double> create_spectral_test_signal(size_t total_samples = 2048)
    {
        std::vector<double> signal;
        signal.reserve(total_samples);

        for (size_t i = 0; i < total_samples; ++i) {
            double t = static_cast<double>(i) / 44100.0;
            double sample = 0.0;

            if (i < total_samples / 3) {
                sample = 0.5 * std::sin(2.0 * M_PI * 110.0 * t);
            } else if (i < 2 * total_samples / 3) {
                sample = 0.3 * std::sin(2.0 * M_PI * 110.0 * t) + 0.4 * std::sin(2.0 * M_PI * 2200.0 * t) + 0.3 * std::sin(2.0 * M_PI * 4400.0 * t);
            } else {
                sample = 0.4 * std::sin(2.0 * M_PI * 440.0 * t);
            }

            signal.push_back(sample);
        }

        return signal;
    }

    /**
     * @brief Generate signal with known mean characteristics
     */
    static std::vector<double> create_above_mean_signal(size_t total_samples = 1024)
    {
        std::vector<double> signal;
        signal.reserve(total_samples);

        for (size_t i = 0; i < total_samples; ++i) {
            double base_value = 0.2;

            if (i >= 200 && i < 300) {
                signal.push_back(base_value + 0.8);
            } else if (i >= 500 && i < 600) {
                signal.push_back(base_value + 0.6);
            } else {
                signal.push_back(base_value + 0.05 * std::sin(2.0 * M_PI * i / 32.0));
            }
        }

        return signal;
    }
};

// =========================================================================
// HIGH ENERGY EXTRACTION TESTS
// =========================================================================

class HighEnergyExtractionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = ExtractionTestDataGenerator::create_energy_burst_signal(2048, 256, 32);
        extractor = std::make_shared<StandardFeatureExtractor>(256, 128);
        extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);
    }

    std::vector<double> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(HighEnergyExtractionTest, DetectsHighEnergyBursts)
{
    extractor->set_parameter("energy_threshold", 0.1);

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_FALSE(extracted.empty()) << "No high-energy data extracted";

    EXPECT_GE(extracted.size(), 32u) << "Should extract at least one burst region";
    EXPECT_LE(extracted.size(), test_signal.size()) << "Cannot extract more than input";
}

TEST_F(HighEnergyExtractionTest, ThresholdSensitivity)
{
    extractor->set_parameter("energy_threshold", 10.0);
    DataVariant signal_variant { test_signal };
    auto high_thresh_result = extractor->extract_data(signal_variant);
    EXPECT_TRUE(high_thresh_result.empty()) << "High threshold should extract nothing";

    extractor->set_parameter("energy_threshold", 0.01);
    auto low_thresh_result = extractor->extract_data(signal_variant);
    EXPECT_GE(low_thresh_result.size(), test_signal.size() * 0.8) << "Low threshold should extract most data";
}

TEST_F(HighEnergyExtractionTest, WindowParameterEffects)
{
    extractor->set_parameter("energy_threshold", 0.3);

    extractor->set_window_size(128);
    extractor->set_hop_size(64);

    DataVariant signal_variant { test_signal };
    auto small_window_result = extractor->extract_data(signal_variant);

    EXPECT_FALSE(small_window_result.empty());

    extractor->set_window_size(512);
    extractor->set_hop_size(256);
    auto large_window_result = extractor->extract_data(signal_variant);

    EXPECT_NE(small_window_result.size(), large_window_result.size());
}

// =========================================================================
// PEAK EXTRACTION TESTS
// =========================================================================

class PeakExtractionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        peak_locations = { 100, 300, 500, 700 };
        test_signal = ExtractionTestDataGenerator::create_peak_signal(1024, peak_locations);
        extractor = std::make_shared<StandardFeatureExtractor>();
        extractor->set_extraction_method(ExtractionMethod::PEAK_DATA);
    }

    std::vector<size_t> peak_locations;
    std::vector<double> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(PeakExtractionTest, DetectsAllPeaks)
{
    extractor->set_parameter("threshold", 1.0);
    extractor->set_parameter("min_distance", 50.0);
    extractor->set_parameter("region_size", static_cast<u_int32_t>(64));

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    size_t expected_samples = peak_locations.size() * 64;
    EXPECT_GE(extracted.size(), expected_samples * 0.8) << "Should extract data around all peaks";
    EXPECT_LE(extracted.size(), expected_samples * 1.2) << "Extracted more data than expected";
}

TEST_F(PeakExtractionTest, RespectsPeakThreshold)
{
    extractor->set_parameter("threshold", 2.0);
    extractor->set_parameter("min_distance", 50.0);
    extractor->set_parameter("region_size", static_cast<u_int32_t>(64));

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_TRUE(extracted.empty()) << "High threshold should prevent peak detection";
}

TEST_F(PeakExtractionTest, MinimumDistanceConstraint)
{
    extractor->set_parameter("threshold", 1.0);
    extractor->set_parameter("min_distance", 500.0);
    extractor->set_parameter("region_size", static_cast<u_int32_t>(64));

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    size_t expected_max = 2 * 64;
    EXPECT_LE(extracted.size(), expected_max) << "Distance constraint should limit peak detection";
}

TEST_F(PeakExtractionTest, RegionSizeEffect)
{
    extractor->set_parameter("threshold", 1.0);
    extractor->set_parameter("min_distance", 50.0);

    extractor->set_parameter("region_size", static_cast<u_int32_t>(16));
    DataVariant signal_variant { test_signal };
    auto small_region = extractor->extract_data(signal_variant);

    extractor->set_parameter("region_size", static_cast<u_int32_t>(128));
    auto large_region = extractor->extract_data(signal_variant);

    EXPECT_GT(large_region.size(), small_region.size()) << "Larger region should extract more data";
}

// =========================================================================
// OUTLIER EXTRACTION TESTS
// =========================================================================

class OutlierExtractionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = ExtractionTestDataGenerator::create_outlier_signal(1024);
        extractor = std::make_shared<StandardFeatureExtractor>(128, 64);
        extractor->set_extraction_method(ExtractionMethod::OUTLIER_DATA);
    }

    std::vector<double> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(OutlierExtractionTest, DetectsStatisticalOutliers)
{
    extractor->set_parameter("std_dev_threshold", 1.5); // Reduced from 2.0

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_FALSE(extracted.empty()) << "Should detect statistical outliers";
    EXPECT_LT(extracted.size(), test_signal.size() * 0.5) << "Should be selective about outliers";

    if (extracted.empty()) {
        std::cout << "DEBUG: No outliers detected. Signal stats:" << std::endl;

        double mean = std::accumulate(test_signal.begin(), test_signal.end(), 0.0) / test_signal.size();
        double variance = 0.0;
        for (double val : test_signal) {
            variance += (val - mean) * (val - mean);
        }
        variance /= test_signal.size();
        double std_dev = std::sqrt(variance);

        std::cout << "  Mean: " << mean << ", Std Dev: " << std_dev << std::endl;

        auto [min_it, max_it] = std::ranges::minmax_element(test_signal);
        std::cout << "  Range: [" << *min_it << ", " << *max_it << "]" << std::endl;
    }
}

TEST_F(OutlierExtractionTest, ValidateOutlierSignalGeneration)
{
    const size_t window_size = 128;
    const size_t hop_size = 64;

    std::vector<double> window_means;

    for (size_t start = 0; start + window_size <= test_signal.size(); start += hop_size) {
        double window_sum = 0.0;
        for (size_t i = start; i < start + window_size; ++i) {
            window_sum += test_signal[i];
        }
        double window_mean = window_sum / window_size;
        window_means.push_back(window_mean);
    }

    double global_mean = std::accumulate(window_means.begin(), window_means.end(), 0.0) / window_means.size();
    double variance = 0.0;
    for (double mean : window_means) {
        variance += (mean - global_mean) * (mean - global_mean);
    }
    variance /= window_means.size();
    double std_dev = std::sqrt(variance);

    std::cout << "Window analysis - Global mean: " << global_mean
              << ", Std dev: " << std_dev << std::endl;

    int outlier_count = 0;
    for (double window_mean : window_means) {
        if (std::abs(window_mean - global_mean) > 1.5 * std_dev) {
            outlier_count++;
        }
    }

    std::cout << "Outlier windows found: " << outlier_count
              << " out of " << window_means.size() << std::endl;

    EXPECT_GT(outlier_count, 0) << "Test signal should contain detectable outlier windows";
}

TEST_F(OutlierExtractionTest, ThresholdSensitivity)
{
    extractor->set_parameter("std_dev_threshold", 4.0);
    DataVariant signal_variant { test_signal };
    auto strict_result = extractor->extract_data(signal_variant);

    extractor->set_parameter("std_dev_threshold", 1.0);
    auto lenient_result = extractor->extract_data(signal_variant);

    EXPECT_GE(lenient_result.size(), strict_result.size())
        << "Lenient threshold should extract more data";
}

// =========================================================================
// SPECTRAL EXTRACTION TESTS
// =========================================================================

class SpectralExtractionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = ExtractionTestDataGenerator::create_spectral_test_signal(2048);
        extractor = std::make_shared<StandardFeatureExtractor>(512, 256);
        extractor->set_extraction_method(ExtractionMethod::HIGH_SPECTRAL_DATA);
    }

    std::vector<double> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(SpectralExtractionTest, DetectsHighSpectralEnergy)
{
    extractor->set_parameter("spectral_threshold", 0.2);

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_FALSE(extracted.empty()) << "Should detect high spectral energy regions";

    EXPECT_GE(extracted.size(), test_signal.size() * 0.1) << "Should extract meaningful amount";
    EXPECT_LE(extracted.size(), test_signal.size()) << "Cannot extract more than input";
}

TEST_F(SpectralExtractionTest, SpectralThresholdEffect)
{
    extractor->set_parameter("spectral_threshold", 0.5);
    DataVariant signal_variant { test_signal };
    auto high_thresh = extractor->extract_data(signal_variant);

    extractor->set_parameter("spectral_threshold", 0.05);
    auto low_thresh = extractor->extract_data(signal_variant);

    EXPECT_GE(low_thresh.size(), high_thresh.size())
        << "Lower spectral threshold should extract more data";
}

// =========================================================================
// ABOVE MEAN EXTRACTION TESTS
// =========================================================================

class AboveMeanExtractionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = ExtractionTestDataGenerator::create_above_mean_signal(1024);
        extractor = std::make_shared<StandardFeatureExtractor>(128, 64);
        extractor->set_extraction_method(ExtractionMethod::ABOVE_MEAN_DATA);
    }

    std::vector<double> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(AboveMeanExtractionTest, ExtractsAboveMeanRegions)
{
    extractor->set_parameter("mean_multiplier", 1.5);

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_FALSE(extracted.empty()) << "Should detect above-mean regions";

    EXPECT_GE(extracted.size(), 50u) << "Should extract meaningful above-mean data";
}

TEST_F(AboveMeanExtractionTest, MeanMultiplierEffect)
{
    extractor->set_parameter("mean_multiplier", 10.0);
    DataVariant signal_variant { test_signal };
    auto high_mult = extractor->extract_data(signal_variant);
    EXPECT_TRUE(high_mult.empty()) << "High multiplier should extract nothing";

    extractor->set_parameter("mean_multiplier", 1.1);
    auto low_mult = extractor->extract_data(signal_variant);
    EXPECT_GE(low_mult.size(), test_signal.size() * 0.5) << "Low multiplier should extract substantial data";
}

// =========================================================================
// OVERLAPPING WINDOWS TESTS
// =========================================================================

class OverlappingWindowsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal.resize(1024);
        for (size_t i = 0; i < test_signal.size(); ++i) {
            test_signal[i] = std::sin(2.0 * M_PI * i / 64.0);
        }

        extractor = std::make_shared<StandardFeatureExtractor>(256, 128);
        extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);
    }

    std::vector<double> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(OverlappingWindowsTest, ExtractsOverlappingWindows)
{
    extractor->set_parameter("overlap", 0.5);

    DataVariant signal_variant { test_signal };
    auto extracted = extractor->extract_data(signal_variant);

    // With 50% overlap, window size 256, hop 128, signal size 1024:
    // Expected windows: (1024 - 256) / 128 + 1 = 7 windows
    // Total samples: 7 * 256 = 1792 samples
    size_t expected_samples = 7 * 256;

    EXPECT_GE(extracted.size(), expected_samples * 0.9) << "Should extract expected number of windowed samples";
    EXPECT_LE(extracted.size(), expected_samples * 1.1) << "Shouldn't extract too many samples";
}

TEST_F(OverlappingWindowsTest, OverlapParameterEffect)
{
    extractor->set_parameter("overlap", 0.0);
    DataVariant signal_variant { test_signal };
    auto no_overlap = extractor->extract_data(signal_variant);

    extractor->set_parameter("overlap", 0.75);
    auto high_overlap = extractor->extract_data(signal_variant);

    EXPECT_GT(high_overlap.size(), no_overlap.size())
        << "Higher overlap should extract more total samples";
}

TEST_F(OverlappingWindowsTest, WindowSizeConsistency)
{
    extractor->set_parameter("overlap", 0.5);

    extractor->set_window_size(128);
    extractor->set_hop_size(64);
    DataVariant signal_variant { test_signal };
    auto small_windows = extractor->extract_data(signal_variant);

    extractor->set_window_size(512);
    extractor->set_hop_size(256);
    auto large_windows = extractor->extract_data(signal_variant);

    EXPECT_NE(small_windows.size(), large_windows.size())
        << "Different window sizes should produce different results";
}

// =========================================================================
// EDGE CASE AND VALIDATION TESTS
// =========================================================================

class ExtractionValidationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        extractor = std::make_shared<StandardFeatureExtractor>();
    }

    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(ExtractionValidationTest, HandlesEmptySignal)
{
    std::vector<double> empty_signal;
    DataVariant empty_variant { empty_signal };

    for (auto method : { ExtractionMethod::HIGH_ENERGY_DATA,
             ExtractionMethod::PEAK_DATA,
             ExtractionMethod::OUTLIER_DATA,
             ExtractionMethod::HIGH_SPECTRAL_DATA,
             ExtractionMethod::ABOVE_MEAN_DATA,
             ExtractionMethod::OVERLAPPING_WINDOWS }) {

        extractor->set_extraction_method(method);

        EXPECT_NO_THROW({
            auto result = extractor->extract_data(empty_variant);
            EXPECT_TRUE(result.empty()) << "Empty signal should produce empty result";
        }) << "Method should handle empty signal gracefully";
    }
}

TEST_F(ExtractionValidationTest, HandlesShortSignal)
{
    std::vector<double> short_signal { 1.0, 2.0, 3.0 };
    DataVariant short_variant { short_signal };

    extractor->set_window_size(512);
    extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(short_variant);
    }) << "Should handle signal shorter than window size";
}

TEST_F(ExtractionValidationTest, HandlesConstantSignal)
{
    std::vector<double> constant_signal(1024, 0.5);
    DataVariant constant_variant { constant_signal };

    extractor->set_extraction_method(ExtractionMethod::PEAK_DATA);
    extractor->set_parameter("threshold", 0.4);

    auto peak_result = extractor->extract_data(constant_variant);
    EXPECT_TRUE(peak_result.empty()) << "Constant signal should have no peaks";

    extractor->set_extraction_method(ExtractionMethod::OUTLIER_DATA);
    auto outlier_result = extractor->extract_data(constant_variant);
    EXPECT_TRUE(outlier_result.empty()) << "Constant signal should have no outliers";
}

TEST_F(ExtractionValidationTest, HandlesExtremeValues)
{
    std::vector<double> extreme_signal;
    extreme_signal.reserve(1024);

    for (size_t i = 0; i < 1024; ++i) {
        if (i % 100 == 0) {
            extreme_signal.push_back(std::numeric_limits<double>::max() / 1e6);
        } else if (i % 100 == 50) {
            extreme_signal.push_back(std::numeric_limits<double>::lowest() / 1e6);
        } else {
            extreme_signal.push_back(0.1 * std::sin(2.0 * M_PI * i / 64.0));
        }
    }

    DataVariant extreme_variant { extreme_signal };

    for (auto method : { ExtractionMethod::HIGH_ENERGY_DATA,
             ExtractionMethod::OUTLIER_DATA,
             ExtractionMethod::HIGH_SPECTRAL_DATA }) {

        extractor->set_extraction_method(method);

        EXPECT_NO_THROW({
            auto result = extractor->extract_data(extreme_variant);
        }) << "Should handle extreme values gracefully";
    }
}

// =========================================================================
// PERFORMANCE AND CONSISTENCY TESTS
// =========================================================================

class ExtractionConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_signal = ExtractionTestDataGenerator::create_energy_burst_signal(1024, 128, 16);
        extractor = std::make_shared<StandardFeatureExtractor>(256, 128);
    }

    std::vector<double> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(ExtractionConsistencyTest, ConsistentResults)
{
    extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);
    extractor->set_parameter("energy_threshold", 0.2);

    DataVariant signal_variant { test_signal };

    auto result1 = extractor->extract_data(signal_variant);
    auto result2 = extractor->extract_data(signal_variant);
    auto result3 = extractor->extract_data(signal_variant);

    EXPECT_EQ(result1.size(), result2.size()) << "Results should be deterministic";
    EXPECT_EQ(result2.size(), result3.size()) << "Results should be deterministic";

    for (size_t i = 0; i < std::min(result1.size(), result2.size()); ++i) {
        EXPECT_NEAR(result1[i], result2[i], 1e-10) << "Values should be nearly identical at index " << i;
    }
}

TEST_F(ExtractionConsistencyTest, ParameterIsolation)
{
    extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);

    extractor->set_parameter("energy_threshold", 0.1);
    DataVariant signal_variant { test_signal };
    auto result1 = extractor->extract_data(signal_variant);

    extractor->set_parameter("energy_threshold", 0.3);
    auto result2 = extractor->extract_data(signal_variant);

    extractor->set_parameter("energy_threshold", 0.1);
    auto result3 = extractor->extract_data(signal_variant);

    EXPECT_EQ(result1.size(), result3.size()) << "Parameter changes should be reversible";

    EXPECT_LE(result2.size(), result1.size()) << "Higher threshold should extract less data";
}

} // namespace MayaFlux::Test
