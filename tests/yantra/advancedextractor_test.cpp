#include "../test_config.h"

#include "MayaFlux/Yantra/Extractors/ExtractionHelper.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"

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
    static std::vector<std::vector<double>> create_energy_burst_signal(size_t total_samples = 2048,
        size_t burst_interval = 256,
        size_t burst_duration = 32,
        size_t num_channels = 2)
    {
        std::vector<std::vector<double>> signal(num_channels);

        for (size_t i = 0; i < num_channels; i++) {
            signal[i].reserve(total_samples);
            for (size_t j = 0; j < total_samples; ++j) {
                double t = static_cast<double>(j) / 44100.0 * (i + 1);

                double sample = 0.05 * std::sin(2.0 * M_PI * 220.0 * t);

                if ((j % burst_interval) < burst_duration) {
                    sample += 1.5 * std::sin(2.0 * M_PI * 1000.0 * t);
                }

                signal[i].push_back(sample);
            }
        }

        return signal;
    }

    /**
     * @brief Generate signal with known peaks at specific locations
     */
    static std::vector<std::vector<double>> create_peak_signal(size_t total_samples = 1024,
        const std::vector<size_t>& peak_locations = { 100, 300, 500, 700 },
        size_t num_channels = 2)
    {
        std::vector<std::vector<double>> signal(num_channels);

        for (size_t i = 0; i < num_channels; i++) {
            signal[i].resize(total_samples, 0.0);
            for (size_t j = 0; j < total_samples; ++j) {
                signal[i][j] = (i + 1) * 0.05 * std::sin(2.0 * M_PI * j / 64.0);
            }

            for (size_t peak_loc : peak_locations) {
                if (peak_loc < total_samples) {
                    signal[i][peak_loc] = 1.5;

                    if (peak_loc > 0)
                        signal[i][peak_loc - 1] = 0.8;
                    if (peak_loc + 1 < total_samples)
                        signal[i][peak_loc + 1] = 0.8;
                }
            }
        }

        return signal;
    }

    /**
     * @brief Generate signal with statistical outliers at known positions
     */
    static std::vector<std::vector<double>> create_outlier_signal(size_t total_samples = 1024, size_t num_channels = 2)
    {
        std::vector<std::vector<double>> signal(num_channels);

        for (size_t ch = 0; ch < num_channels; ++ch) {
            signal[ch].reserve(total_samples);

            std::random_device rd;
            std::mt19937 gen(42 + ch); // Different seed per channel for variety
            std::normal_distribution<double> normal_dist(0.0, 0.02);

            for (size_t i = 0; i < total_samples; ++i) {
                signal[ch].push_back(normal_dist(gen));
            }

            std::vector<std::pair<size_t, size_t>> outlier_regions = {
                { 100, 150 },
                { 300, 350 },
                { 600, 650 },
                { 800, 850 }
            };

            for (const auto& [start, end] : outlier_regions) {
                for (size_t i = start; i < end && i < total_samples; ++i) {
                    double outlier_value = (start % 400 == 100) ? 0.8 : -0.8;
                    signal[ch][i] = outlier_value;
                }
            }
        }

        return signal;
    }

    /**
     * @brief Generate signal with known spectral characteristics
     */
    static std::vector<std::vector<double>> create_spectral_test_signal(size_t total_samples = 2048, size_t num_channels = 2)
    {
        std::vector<std::vector<double>> signal(num_channels);

        for (size_t ch = 0; ch < num_channels; ++ch) {
            signal[ch].reserve(total_samples);

            for (size_t i = 0; i < total_samples; ++i) {
                double t = static_cast<double>(i) / 44100.0 * (ch + 1);
                double sample = 0.0;

                if (i < total_samples / 3) {
                    sample = 0.5 * std::sin(2.0 * M_PI * 110.0 * t);
                } else if (i < 2 * total_samples / 3) {
                    sample = 0.3 * std::sin(2.0 * M_PI * 110.0 * t) + 0.4 * std::sin(2.0 * M_PI * 2200.0 * t) + 0.3 * std::sin(2.0 * M_PI * 4400.0 * t);
                } else {
                    sample = 0.4 * std::sin(2.0 * M_PI * 440.0 * t);
                }

                signal[ch].push_back(sample);
            }
        }

        return signal;
    }
    /**
     * @brief Generate signal with known mean characteristics
     */
    static std::vector<std::vector<double>> create_above_mean_signal(size_t total_samples = 1024, size_t num_channels = 2)
    {
        std::vector<std::vector<double>> signal(num_channels);
        for (size_t ch = 0; ch < num_channels; ++ch) {
            signal[ch].reserve(total_samples);
            for (size_t i = 0; i < total_samples; ++i) {
                double base_value = 0.2;
                if (i >= 200 && i < 300) {
                    signal[ch].push_back(base_value + 0.8);
                } else if (i >= 500 && i < 600) {
                    signal[ch].push_back(base_value + 0.6);
                } else {
                    signal[ch].push_back(base_value + 0.05 * std::sin(2.0 * M_PI * i / 32.0));
                }
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

    std::vector<std::vector<double>> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(HighEnergyExtractionTest, DetectsHighEnergyBursts)
{
    extractor->set_parameter("energy_threshold", 0.1);

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_FALSE(extracted.empty()) << "No high-energy channels extracted";
    EXPECT_LE(extracted.size(), test_signal.size()) << "Cannot extract more than input";

    for (size_t i = 0; i < extracted.size(); i++) {
        EXPECT_FALSE(extracted[i].empty()) << "No high-energy data extracted at channel: " << i << "\n";
        EXPECT_GE(extracted[i].size(), 32u) << "Should extract at least one burst region";
        EXPECT_LE(extracted[i].size(), test_signal[i].size()) << "Cannot extract more than input";
    }
}

TEST_F(HighEnergyExtractionTest, ThresholdSensitivity)
{
    extractor->set_parameter("energy_threshold", 10.0);
    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto high_thresh_result = extractor->extract_data(signal_variant);

    for (const auto& channel : high_thresh_result) {
        EXPECT_TRUE(channel.empty()) << "High threshold should extract nothing";
    }

    extractor->set_parameter("energy_threshold", 0.01);
    auto low_thresh_result = extractor->extract_data(signal_variant);

    for (size_t i = 0; i < low_thresh_result.size(); i++) {
        EXPECT_GE(low_thresh_result[i].size(), test_signal[i].size() * 0.8) << "Low threshold should extract most data";
    }
}

TEST_F(HighEnergyExtractionTest, WindowParameterEffects)
{
    extractor->set_parameter("energy_threshold", 0.3);

    extractor->set_window_size(128);
    extractor->set_hop_size(64);

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto small_window_result = extractor->extract_data(signal_variant);

    for (const auto& channel : small_window_result) {
        EXPECT_FALSE(channel.empty());
    }

    extractor->set_window_size(512);
    extractor->set_hop_size(256);
    auto large_window_result = extractor->extract_data(signal_variant);

    for (size_t i = 0; i < small_window_result.size(); i++) {
        EXPECT_NE(small_window_result[i].size(), large_window_result[i].size());
    }
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
    std::vector<std::vector<double>> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(PeakExtractionTest, DetectsAllPeaks)
{
    extractor->set_parameter("threshold", 1.0);
    extractor->set_parameter("min_distance", 50.0);
    extractor->set_parameter("region_size", static_cast<uint32_t>(64));

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto extracted = extractor->extract_data(signal_variant);

    for (const auto& channel : extracted) {
        size_t expected_samples = peak_locations.size() * 64;
        EXPECT_GE(channel.size(), expected_samples * 0.8) << "Should extract data around all peaks";
        EXPECT_LE(channel.size(), expected_samples * 1.2) << "Extracted more data than expected";
    }
}

TEST_F(PeakExtractionTest, RespectsPeakThreshold)
{
    extractor->set_parameter("threshold", 2.0);
    extractor->set_parameter("min_distance", 50.0);
    extractor->set_parameter("region_size", static_cast<uint32_t>(64));

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto extracted = extractor->extract_data(signal_variant);

    for (const auto& channel : extracted) {
        EXPECT_TRUE(channel.empty()) << "High threshold should prevent peak detection";
    }
}

TEST_F(PeakExtractionTest, MinimumDistanceConstraint)
{
    extractor->set_parameter("threshold", 1.0);
    extractor->set_parameter("min_distance", 500.0);
    extractor->set_parameter("region_size", static_cast<uint32_t>(64));

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto extracted = extractor->extract_data(signal_variant);

    size_t expected_max = 2 * 64;

    for (const auto& channel : extracted) {
        EXPECT_LE(channel.size(), expected_max) << "Distance constraint should limit peak detection";
    }
}

TEST_F(PeakExtractionTest, RegionSizeEffect)
{
    extractor->set_parameter("threshold", 1.0);
    extractor->set_parameter("min_distance", 50.0);

    extractor->set_parameter("region_size", static_cast<uint32_t>(16));
    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto small_region = extractor->extract_data(signal_variant);

    extractor->set_parameter("region_size", static_cast<uint32_t>(128));
    auto large_region = extractor->extract_data(signal_variant);

    for (size_t i = 0; i < large_region.size(); i++) {
        EXPECT_GT(large_region[i].size(), small_region[i].size()) << "Larger region should extract more data";
    }
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

    std::vector<std::vector<double>> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(OutlierExtractionTest, DetectsStatisticalOutliers)
{
    extractor->set_parameter("std_dev_threshold", 1.5); // Reduced from 2.0

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_FALSE(extracted.empty()) << "Should detect statistical outliers";

    for (size_t i = 0; i < extracted.size(); i++) {
        EXPECT_FALSE(extracted[i].empty()) << "Should detect statistical outliers";
        EXPECT_LT(extracted[i].size(), test_signal[i].size() * 0.5) << "Should be selective about outliers";

        if (extracted[i].empty()) {
            std::cout << "DEBUG: No outliers detected. Signal stats:" << '\n';

            double mean = std::accumulate(test_signal[i].begin(), test_signal[i].end(), 0.0) / test_signal[i].size();
            double variance = 0.0;
            for (double val : test_signal[i]) {
                variance += (val - mean) * (val - mean);
            }
            variance /= test_signal[i].size();
            double std_dev = std::sqrt(variance);

            std::cout << "  Mean: " << mean << ", Std Dev: " << std_dev << '\n';

            auto [min_it, max_it] = std::ranges::minmax_element(test_signal[i]);
            std::cout << "  Range: [" << *min_it << ", " << *max_it << "]" << std::endl;
        }
    }
}

TEST_F(OutlierExtractionTest, ValidateOutlierSignalGeneration)
{
    const size_t window_size = 128;
    const size_t hop_size = 64;

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        std::vector<double> window_means;

        for (size_t start = 0; start + window_size <= test_signal[ch].size(); start += hop_size) {
            double window_sum = 0.0;
            for (size_t i = start; i < start + window_size; ++i) {
                window_sum += test_signal[ch][i];
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

        std::cout << "Channel " << ch << " - Window analysis - Global mean: " << global_mean
                  << ", Std dev: " << std_dev << std::endl;

        int outlier_count = 0;
        for (double window_mean : window_means) {
            if (std::abs(window_mean - global_mean) > 1.5 * std_dev) {
                outlier_count++;
            }
        }

        std::cout << "Channel " << ch << " - Outlier windows found: " << outlier_count
                  << " out of " << window_means.size() << std::endl;

        EXPECT_GT(outlier_count, 0) << "Test signal should contain detectable outlier windows in channel " << ch;
    }
}

TEST_F(OutlierExtractionTest, ThresholdSensitivity)
{
    extractor->set_parameter("std_dev_threshold", 4.0);
    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto strict_result = extractor->extract_data(signal_variant);

    extractor->set_parameter("std_dev_threshold", 1.0);
    auto lenient_result = extractor->extract_data(signal_variant);

    for (size_t i = 0; i < strict_result.size(); i++) {
        EXPECT_GE(lenient_result[i].size(), strict_result[i].size())
            << "Lenient threshold should extract more data";
    }
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

    std::vector<std::vector<double>> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(SpectralExtractionTest, DetectsHighSpectralEnergy)
{
    extractor->set_parameter("spectral_threshold", 0.2);

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto extracted = extractor->extract_data(signal_variant);

    EXPECT_FALSE(extracted.empty()) << "Should detect high spectral energy regions";

    for (size_t i = 0; i < extracted.size(); i++) {
        EXPECT_FALSE(extracted[i].empty()) << "Should detect high spectral energy regions";

        EXPECT_GE(extracted[i].size(), test_signal[i].size() * 0.1) << "Should extract meaningful amount";
        EXPECT_LE(extracted[i].size(), test_signal[i].size()) << "Cannot extract more than input";
    }
}

TEST_F(SpectralExtractionTest, SpectralThresholdEffect)
{
    extractor->set_parameter("spectral_threshold", 0.5);
    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto high_thresh = extractor->extract_data(signal_variant);

    extractor->set_parameter("spectral_threshold", 0.05);
    auto low_thresh = extractor->extract_data(signal_variant);

    for (size_t i = 0; i < low_thresh.size(); i++) {
        EXPECT_GE(low_thresh[i].size(), high_thresh[i].size())
            << "Lower spectral threshold should extract more data";
    }
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

    std::vector<std::vector<double>> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(AboveMeanExtractionTest, ExtractsAboveMeanRegions)
{
    extractor->set_parameter("mean_multiplier", 1.5);

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }
    auto extracted = extractor->extract_data(signal_variant);

    for (const auto& channel : extracted) {
        EXPECT_FALSE(channel.empty()) << "Should detect above-mean regions";

        EXPECT_GE(channel.size(), 50U) << "Should extract meaningful above-mean data";
    }
}

TEST_F(AboveMeanExtractionTest, MeanMultiplierEffect)
{
    extractor->set_parameter("mean_multiplier", 10.0);

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto high_mult = extractor->extract_data(signal_variant);
    for (const auto& channel : high_mult) {
        EXPECT_TRUE(channel.empty()) << "High multiplier should extract nothing";
    }

    extractor->set_parameter("mean_multiplier", 1.1);
    auto low_mult = extractor->extract_data(signal_variant);

    for (size_t ch = 0; ch < test_signal.size(); ++ch) {
        const auto& channel = test_signal[ch];
        size_t window_size = extractor->get_window_size();
        size_t hop_size = extractor->get_hop_size();

        double global_sum = 0.0;
        for (double v : channel)
            global_sum += v;
        double global_mean = global_sum / channel.size();
        double threshold = global_mean * 1.1;

        std::vector<std::pair<size_t, size_t>> qualifying_windows;
        for (size_t start = 0; start + window_size <= channel.size(); start += hop_size) {
            double sum = 0.0;
            for (size_t i = start; i < start + window_size; ++i)
                sum += channel[i];
            double mean = sum / window_size;
            if (mean > threshold) {
                qualifying_windows.emplace_back(start, start + window_size);
            }
        }

        std::vector<std::pair<size_t, size_t>> merged;
        if (!qualifying_windows.empty()) {
            merged.push_back(qualifying_windows[0]);
            for (size_t i = 1; i < qualifying_windows.size(); ++i) {
                auto& last = merged.back();
                if (qualifying_windows[i].first <= last.second) {
                    last.second = std::max(last.second, qualifying_windows[i].second);
                } else {
                    merged.push_back(qualifying_windows[i]);
                }
            }
        }

        size_t expected_samples = 0;
        for (const auto& [start, end] : merged)
            expected_samples += (end - start);

        EXPECT_NEAR(low_mult[ch].size(), expected_samples, 2)
            << "Low multiplier should extract all above-mean regions in channel " << ch;
    }
}

// =========================================================================
// OVERLAPPING WINDOWS TESTS
// =========================================================================

class OverlappingWindowsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        size_t num_channels = 2;
        size_t total_samples = 1024;
        test_signal.resize(num_channels);
        for (size_t ch = 0; ch < num_channels; ++ch) {
            test_signal[ch].resize(total_samples);
            for (size_t i = 0; i < total_samples; ++i) {
                test_signal[ch][i] = std::sin(2.0 * M_PI * i / 64.0 + ch); // Slightly different phase per channel
            }
        }

        extractor = std::make_shared<StandardFeatureExtractor>(256, 128);
        extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);
    }

    std::vector<std::vector<double>> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(OverlappingWindowsTest, ExtractsOverlappingWindows)
{
    extractor->set_parameter("overlap", 0.5);

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }
    auto extracted = extractor->extract_data(signal_variant);

    // With 50% overlap, window size 256, hop 128, signal size 1024:
    // Expected windows: (1024 - 256) / 128 + 1 = 7 windows
    // Total samples: 7 * 256 = 1792 samples
    size_t expected_windows = (test_signal[0].size() - 256) / 128 + 1;
    size_t expected_samples = expected_windows * 256;

    for (size_t ch = 0; ch < extracted.size(); ++ch) {
        EXPECT_GE(extracted[ch].size(), expected_samples * 0.9) << "Should extract expected number of windowed samples in channel " << ch;
        EXPECT_LE(extracted[ch].size(), expected_samples * 1.1) << "Shouldn't extract too many samples in channel " << ch;
    }
}

TEST_F(OverlappingWindowsTest, OverlapParameterEffect)
{
    extractor->set_parameter("overlap", 0.0);
    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }
    auto no_overlap = extractor->extract_data(signal_variant);

    extractor->set_parameter("overlap", 0.75);
    auto high_overlap = extractor->extract_data(signal_variant);

    for (size_t ch = 0; ch < high_overlap.size(); ++ch) {
        EXPECT_GT(high_overlap[ch].size(), no_overlap[ch].size())
            << "Higher overlap should extract more total samples in channel " << ch;
    }
}

TEST_F(OverlappingWindowsTest, WindowSizeConsistency)
{
    extractor->set_parameter("overlap", 0.5);

    extractor->set_window_size(128);
    extractor->set_hop_size(64);
    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }
    auto small_windows = extractor->extract_data(signal_variant);

    extractor->set_window_size(512);
    extractor->set_hop_size(256);
    auto large_windows = extractor->extract_data(signal_variant);

    for (size_t ch = 0; ch < small_windows.size(); ++ch) {
        EXPECT_NE(small_windows[ch].size(), large_windows[ch].size())
            << "Different window sizes should produce different results in channel " << ch;
    }
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
    std::vector<std::vector<double>> empty_signal(2);

    std::vector<DataVariant> empty_variant(empty_signal.size());

    for (size_t i = 0; i < empty_signal.size(); ++i) {
        empty_variant[i] = empty_signal[i];
    }

    for (auto method : { ExtractionMethod::HIGH_ENERGY_DATA,
             ExtractionMethod::PEAK_DATA,
             ExtractionMethod::OUTLIER_DATA,
             ExtractionMethod::HIGH_SPECTRAL_DATA,
             ExtractionMethod::ABOVE_MEAN_DATA,
             ExtractionMethod::OVERLAPPING_WINDOWS }) {

        extractor->set_extraction_method(method);

        EXPECT_NO_THROW({
            auto result = extractor->extract_data(empty_variant);
            EXPECT_FALSE(result.empty()) << "Empty signal should produce empty result";
            for (auto& channel : result) {
                EXPECT_TRUE(channel.empty()) << "Empty signal should produce empty result";
            }
        }) << "Method should handle empty signal gracefully";
    }
}

TEST_F(ExtractionValidationTest, HandlesShortSignal)
{
    std::vector<std::vector<double>> short_signal(2, std::vector<double> { 1.0, 2.0, 3.0 });
    std::vector<DataVariant> short_variant(short_signal.size());
    for (size_t i = 0; i < short_signal.size(); ++i) {
        short_variant[i] = short_signal[i];
    }

    extractor->set_window_size(512);
    extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(short_variant);
        EXPECT_FALSE(result.empty()) << "Should handle signal shorter than window size";
        for (auto& channel : result) {
            // For short signals, result should be empty or contain very few samples
            EXPECT_TRUE(channel.empty() || channel.size() <= short_signal[0].size())
                << "Should not extract more than available samples";
        }
    }) << "Should handle signal shorter than window size";
}

TEST_F(ExtractionValidationTest, HandlesConstantSignal)
{
    std::vector<std::vector<double>> constant_signal(2, std::vector<double>(1024, 0.5));
    std::vector<DataVariant> constant_variant(constant_signal.size());
    for (size_t i = 0; i < constant_signal.size(); ++i) {
        constant_variant[i] = constant_signal[i];
    }

    extractor->set_extraction_method(ExtractionMethod::PEAK_DATA);
    extractor->set_parameter("threshold", 0.4);

    auto peak_result = extractor->extract_data(constant_variant);
    for (const auto& channel : peak_result) {
        EXPECT_TRUE(channel.empty()) << "Constant signal should have no peaks";
    }

    extractor->set_extraction_method(ExtractionMethod::OUTLIER_DATA);
    auto outlier_result = extractor->extract_data(constant_variant);
    for (const auto& channel : outlier_result) {
        EXPECT_TRUE(channel.empty()) << "Constant signal should have no outliers";
    }
}

TEST_F(ExtractionValidationTest, HandlesExtremeValues)
{
    std::vector<std::vector<double>> extreme_signal(2);
    for (size_t ch = 0; ch < 2; ++ch) {
        extreme_signal[ch].reserve(1024);
        for (size_t i = 0; i < 1024; ++i) {
            if (i % 100 == 0) {
                extreme_signal[ch].push_back(std::numeric_limits<double>::max() / 1e6);
            } else if (i % 100 == 50) {
                extreme_signal[ch].push_back(std::numeric_limits<double>::lowest() / 1e6);
            } else {
                extreme_signal[ch].push_back(0.1 * std::sin(2.0 * M_PI * i / 64.0));
            }
        }
    }

    std::vector<DataVariant> extreme_variant(extreme_signal.size());
    for (size_t i = 0; i < extreme_signal.size(); ++i) {
        extreme_variant[i] = extreme_signal[i];
    }

    for (auto method : { ExtractionMethod::HIGH_ENERGY_DATA,
             ExtractionMethod::OUTLIER_DATA,
             ExtractionMethod::HIGH_SPECTRAL_DATA }) {

        extractor->set_extraction_method(method);

        EXPECT_NO_THROW({
            auto result = extractor->extract_data(extreme_variant);
            EXPECT_FALSE(result.empty()) << "Should handle extreme values gracefully";
            // No assertion on content, just that it doesn't crash and returns per-channel results
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

    std::vector<std::vector<double>> test_signal;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(ExtractionConsistencyTest, ConsistentResults)
{
    extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);
    extractor->set_parameter("energy_threshold", 0.2);

    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }

    auto result1 = extractor->extract_data(signal_variant);
    auto result2 = extractor->extract_data(signal_variant);
    auto result3 = extractor->extract_data(signal_variant);

    ASSERT_EQ(result1.size(), result2.size()) << "Results should be deterministic (channel count)";
    ASSERT_EQ(result2.size(), result3.size()) << "Results should be deterministic (channel count)";

    for (size_t ch = 0; ch < result1.size(); ++ch) {
        EXPECT_EQ(result1[ch].size(), result2[ch].size()) << "Results should be deterministic in channel " << ch;
        EXPECT_EQ(result2[ch].size(), result3[ch].size()) << "Results should be deterministic in channel " << ch;
        for (size_t i = 0; i < std::min(result1[ch].size(), result2[ch].size()); ++i) {
            EXPECT_NEAR(result1[ch][i], result2[ch][i], 1e-10)
                << "Values should be nearly identical at index " << i << " in channel " << ch;
        }
    }
}

TEST_F(ExtractionConsistencyTest, ParameterIsolation)
{
    extractor->set_extraction_method(ExtractionMethod::HIGH_ENERGY_DATA);

    extractor->set_parameter("energy_threshold", 0.1);
    std::vector<DataVariant> signal_variant(test_signal.size());
    for (size_t i = 0; i < test_signal.size(); ++i) {
        signal_variant[i] = test_signal[i];
    }
    auto result1 = extractor->extract_data(signal_variant);

    extractor->set_parameter("energy_threshold", 0.3);
    auto result2 = extractor->extract_data(signal_variant);

    extractor->set_parameter("energy_threshold", 0.1);
    auto result3 = extractor->extract_data(signal_variant);

    ASSERT_EQ(result1.size(), result3.size()) << "Parameter changes should be reversible (channel count)";

    for (size_t ch = 0; ch < result1.size(); ++ch) {
        EXPECT_EQ(result1[ch].size(), result3[ch].size()) << "Parameter changes should be reversible in channel " << ch;
        EXPECT_LE(result2[ch].size(), result1[ch].size()) << "Higher threshold should extract less data in channel " << ch;
    }
}

} // namespace MayaFlux::Test
