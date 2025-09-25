#include "../test_config.h"

#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class FeatureExtractorBasicTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        multi_channel_data = create_multi_channel_audio_data();
        extractor = std::make_shared<StandardFeatureExtractor>(512, 256);
    }

    static std::vector<std::vector<double>> create_multi_channel_audio_data()
    {
        std::vector<std::vector<double>> channels(2);
        channels[0].reserve(2048);
        channels[1].reserve(2048);

        for (size_t i = 0; i < 2048; ++i) {
            double t = static_cast<double>(i) / 44100.0;

            // Channel 1: 440 Hz + bursts + spikes
            double sample1 = 0.2 * std::sin(2.0 * M_PI * 440.0 * t);
            if ((i % 256) < 32) {
                sample1 += 0.8 * std::sin(2.0 * M_PI * 1000.0 * t);
            }
            if (i % 200 == 0) {
                sample1 += (i % 400 == 0) ? 1.2 : -1.2;
            }

            // Channel 2: 880 Hz + different bursts + spikes
            double sample2 = 0.2 * std::sin(2.0 * M_PI * 880.0 * t);
            if ((i % 128) < 16) {
                sample2 += 0.6 * std::sin(2.0 * M_PI * 2000.0 * t);
            }
            if (i % 150 == 0) {
                sample2 += (i % 300 == 0) ? 1.0 : -1.0;
            }

            channels[0].push_back(sample1);
            channels[1].push_back(sample2);
        }

        return channels;
    }

    std::vector<std::vector<double>> multi_channel_data;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(FeatureExtractorBasicTest, ConstructionAndBasicProperties)
{
    EXPECT_NE(extractor, nullptr);
    EXPECT_EQ(extractor->get_extraction_type(), ExtractionType::FEATURE_GUIDED);
    EXPECT_EQ(extractor->get_extractor_name(), "FeatureExtractor");
    EXPECT_EQ(extractor->get_window_size(), 512);
    EXPECT_EQ(extractor->get_hop_size(), 256);
}

TEST_F(FeatureExtractorBasicTest, AvailableMethodsAndEnumHandling)
{
    auto methods = extractor->get_available_methods();
    EXPECT_FALSE(methods.empty());

    std::string method_str = FeatureExtractor<>::method_to_string(ExtractionMethod::HIGH_ENERGY_DATA);
    EXPECT_EQ(method_str, "high_energy_data");

    ExtractionMethod method = FeatureExtractor<>::string_to_method("peak_data");
    EXPECT_EQ(method, ExtractionMethod::PEAK_DATA);

    method = FeatureExtractor<>::string_to_method("OUTLIER_DATA");
    EXPECT_EQ(method, ExtractionMethod::OUTLIER_DATA);
}

TEST_F(FeatureExtractorBasicTest, MethodSetting)
{
    extractor->set_extraction_method(ExtractionMethod::HIGH_SPECTRAL_DATA);
    EXPECT_EQ(extractor->get_extraction_method(), ExtractionMethod::HIGH_SPECTRAL_DATA);

    extractor->set_extraction_method("above_mean_data");
    EXPECT_EQ(extractor->get_extraction_method(), ExtractionMethod::ABOVE_MEAN_DATA);
}

TEST_F(FeatureExtractorBasicTest, WindowParameterHandling)
{
    extractor->set_window_size(1024);
    extractor->set_hop_size(512);

    EXPECT_EQ(extractor->get_window_size(), 1024);
    EXPECT_EQ(extractor->get_hop_size(), 512);

    EXPECT_THROW(extractor->set_hop_size(2048), std::invalid_argument);
}

class FeatureExtractorTypeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        multi_channel_data = {
            { 1.0, 2.0, 3.0, 4.0, 5.0 },
            { 2.0, 4.0, 6.0, 8.0, 10.0 }
        };
    }

    std::vector<std::vector<double>> multi_channel_data;
};

TEST_F(FeatureExtractorTypeTest, DifferentOutputTypes)
{
    auto vector_extractor = std::make_shared<StandardFeatureExtractor>();
    auto eigen_extractor = std::make_shared<MatrixFeatureExtractor>();

    std::vector<Kakshya::DataVariant> multi_channel_input = {
        Kakshya::DataVariant { multi_channel_data[0] },
        Kakshya::DataVariant { multi_channel_data[1] }
    };

    auto result_vec = vector_extractor->extract_data(multi_channel_input);
    EXPECT_EQ(result_vec.size(), 2);
    for (const auto& ch : result_vec) {
        EXPECT_FALSE(ch.empty());
    }

    auto result_eigen = eigen_extractor->extract_data(multi_channel_input);
    EXPECT_EQ(result_eigen.cols(), 2);
    for (int c = 0; c < result_eigen.cols(); ++c) {
        EXPECT_GT(result_eigen.col(c).norm(), 0.0);
    }
}

class FeatureExtractorFunctionalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        multi_channel_data = create_multi_channel_audio_data();
        extractor = std::make_shared<StandardFeatureExtractor>(256, 128);
    }

    static std::vector<std::vector<double>> create_multi_channel_audio_data()
    {
        std::vector<std::vector<double>> channels(2);
        channels[0].reserve(1024);
        channels[1].reserve(1024);

        for (size_t i = 0; i < 1024; ++i) {
            double sample1 = 0.1 * std::sin(2.0 * M_PI * i / 32.0);
            if ((i % 128) < 64)
                sample1 += 0.3;
            double sample2 = 0.2 * std::sin(2.0 * M_PI * i / 64.0);
            if ((i % 256) < 32)
                sample2 += 0.5;
            channels[0].push_back(sample1);
            channels[1].push_back(sample2);
        }
        return channels;
    }

    std::vector<std::vector<double>> multi_channel_data;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(FeatureExtractorFunctionalTest, OverlappingWindowsExtraction)
{
    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);
    extractor->set_parameter("overlap", 0.5);

    std::vector<Kakshya::DataVariant> multi_channel_input = {
        Kakshya::DataVariant { multi_channel_data[0] },
        Kakshya::DataVariant { multi_channel_data[1] }
    };

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(multi_channel_input);
        EXPECT_EQ(result.size(), 2);
        for (const auto& ch : result) {
            EXPECT_FALSE(ch.empty());
        }
        EXPECT_NE(result[0], result[1]);
    });
}

TEST_F(FeatureExtractorFunctionalTest, ParameterManagement)
{
    extractor->set_parameter("energy_threshold", 0.25);
    extractor->set_parameter("threshold", 0.15);
    extractor->set_parameter("min_distance", 20.0);

    auto energy_param = extractor->get_parameter("energy_threshold");
    EXPECT_TRUE(energy_param.has_value());

    auto default_val = extractor->get_parameter_or_default<double>("nonexistent", 99.9);
    EXPECT_EQ(default_val, 99.9);
}

TEST_F(FeatureExtractorFunctionalTest, InputValidation)
{
    std::vector<Kakshya::DataVariant> multi_channel_input = {
        Kakshya::DataVariant { multi_channel_data[0] },
        Kakshya::DataVariant { multi_channel_data[1] }
    };

    IO<std::vector<Kakshya::DataVariant>> valid_input(multi_channel_input);
    EXPECT_TRUE(extractor->validate_extraction_input(valid_input));

    std::vector<double> empty_data;
    std::vector<Kakshya::DataVariant> empty_multi_channel = {
        Kakshya::DataVariant { empty_data },
        Kakshya::DataVariant { empty_data }
    };
    IO<std::vector<Kakshya::DataVariant>> empty_input(empty_multi_channel);

    EXPECT_NO_THROW(extractor->validate_extraction_input(empty_input));
}

class FeatureExtractorEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        extractor = std::make_shared<StandardFeatureExtractor>();
    }

    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(FeatureExtractorEdgeCaseTest, EmptyDataHandling)
{
    std::vector<double> empty_data;
    std::vector<Kakshya::DataVariant> empty_multi_channel = {
        Kakshya::DataVariant { empty_data },
        Kakshya::DataVariant { empty_data }
    };

    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(empty_multi_channel);
        EXPECT_EQ(result.size(), 2);
        for (const auto& ch : result) {
            EXPECT_TRUE(ch.empty());
        }
    });
}

TEST_F(FeatureExtractorEdgeCaseTest, InvalidEnumConversion)
{
    EXPECT_THROW(
        FeatureExtractor<>::string_to_method("invalid_method_name"),
        std::invalid_argument);
}

TEST_F(FeatureExtractorEdgeCaseTest, ProblematicNumericalData)
{
    std::vector<double> problematic_data {
        1.0, 2.0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -5.0, 0.0
    };

    std::vector<Kakshya::DataVariant> multi_channel_input = {
        Kakshya::DataVariant { problematic_data },
        Kakshya::DataVariant { problematic_data }
    };

    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(multi_channel_input);
        EXPECT_EQ(result.size(), 2);
    });
}

class FeatureExtractorPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        multi_channel_data.resize(2);
        multi_channel_data[0].resize(44100);
        multi_channel_data[1].resize(44100);
        for (size_t i = 0; i < 44100; ++i) {
            multi_channel_data[0][i] = std::sin(2.0 * M_PI * 440.0 * i / 44100.0);
            multi_channel_data[1][i] = std::sin(2.0 * M_PI * 880.0 * i / 44100.0);
        }

        extractor = std::make_shared<StandardFeatureExtractor>(1024, 512);
    }

    std::vector<std::vector<double>> multi_channel_data;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(FeatureExtractorPerformanceTest, LargeDataProcessing)
{
    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);
    extractor->set_parameter("overlap", 0.5);

    std::vector<Kakshya::DataVariant> multi_channel_input = {
        Kakshya::DataVariant { multi_channel_data[0] },
        Kakshya::DataVariant { multi_channel_data[1] }
    };

    auto start_time = std::chrono::high_resolution_clock::now();

    auto result = extractor->extract_data(multi_channel_input);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_EQ(result.size(), 2);
    for (const auto& ch : result) {
        EXPECT_FALSE(ch.empty());
    }
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(FeatureExtractorPerformanceTest, BatchProcessing)
{
    std::vector<std::vector<double>> base_channel(2);
    base_channel[0].resize(44100, 0.0);
    base_channel[1].resize(44100, 0.0);
    for (size_t i = 0; i < 44100; ++i) {
        base_channel[0][i] = std::sin(2.0 * M_PI * 440.0 * i / 44100.0);
        base_channel[1][i] = std::sin(2.0 * M_PI * 880.0 * i / 44100.0);
    }

    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::vector<std::vector<double>>> results;
    for (int i = 0; i < 5; ++i) {
        std::vector<Kakshya::DataVariant> multi_channel_input = {
            Kakshya::DataVariant { base_channel[0] },
            Kakshya::DataVariant { base_channel[1] }
        };
        results.push_back(extractor->extract_data(multi_channel_input));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_EQ(results.size(), 5);
    EXPECT_LT(duration.count(), 5000);

    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_EQ(results[i].size(), results[0].size());
    }
}

} // namespace MayaFlux::Test
