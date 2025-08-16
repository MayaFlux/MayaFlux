#include "../test_config.h"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class FeatureExtractorBasicTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = create_test_audio_data();
        extractor = std::make_shared<StandardFeatureExtractor>(512, 256);
    }

    std::vector<double> create_test_audio_data()
    {
        std::vector<double> audio;
        audio.reserve(2048);

        for (size_t i = 0; i < 2048; ++i) {
            double t = static_cast<double>(i) / 44100.0;

            double sample = 0.2 * std::sin(2.0 * M_PI * 440.0 * t);

            if ((i % 256) < 32) {
                sample += 0.8 * std::sin(2.0 * M_PI * 1000.0 * t);
            }

            if (i % 200 == 0) {
                sample += (i % 400 == 0) ? 1.2 : -1.2;
            }

            audio.push_back(sample);
        }

        return audio;
    }

    std::vector<double> test_data;
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
        test_data = std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 };
    }

    std::vector<double> test_data;
};

TEST_F(FeatureExtractorTypeTest, DifferentOutputTypes)
{
    auto vector_extractor = std::make_shared<StandardFeatureExtractor>();
    EXPECT_NE(vector_extractor, nullptr);

    auto eigen_extractor = std::make_shared<VectorFeatureExtractor>();
    EXPECT_NE(eigen_extractor, nullptr);

    auto variant_extractor = std::make_shared<VariantFeatureExtractor>();
    EXPECT_NE(variant_extractor, nullptr);

    EXPECT_EQ(vector_extractor->get_extraction_type(), ExtractionType::FEATURE_GUIDED);
    EXPECT_EQ(eigen_extractor->get_extraction_type(), ExtractionType::FEATURE_GUIDED);
    EXPECT_EQ(variant_extractor->get_extraction_type(), ExtractionType::FEATURE_GUIDED);
}

class FeatureExtractorFunctionalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = create_synthetic_audio();
        extractor = std::make_shared<StandardFeatureExtractor>(256, 128);
    }

    std::vector<double> create_synthetic_audio()
    {
        std::vector<double> audio;
        audio.reserve(1024);

        for (size_t i = 0; i < 1024; ++i) {
            double sample = 0.1 * std::sin(2.0 * M_PI * i / 32.0);

            if ((i % 128) < 64) {
                sample += 0.3;
            }

            audio.push_back(sample);
        }

        return audio;
    }

    std::vector<double> test_data;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(FeatureExtractorFunctionalTest, OverlappingWindowsExtraction)
{
    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);
    extractor->set_parameter("overlap", 0.5);

    Kakshya::DataVariant audio_variant { test_data };

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(audio_variant);
        EXPECT_FALSE(result.empty());
        EXPECT_LE(result.size(), test_data.size() * 2);
    });
}

TEST_F(FeatureExtractorFunctionalTest, ParameterManagement)
{
    extractor->set_parameter("energy_threshold", 0.25);
    extractor->set_parameter("threshold", 0.15);
    extractor->set_parameter("min_distance", 20.0);

    auto energy_param = extractor->get_parameter("energy_threshold");
    EXPECT_TRUE(energy_param.has_value());

    double default_val = extractor->get_parameter_or_default<double>("nonexistent", 99.9);
    EXPECT_EQ(default_val, 99.9);
}

TEST_F(FeatureExtractorFunctionalTest, InputValidation)
{
    Kakshya::DataVariant valid_variant { test_data };

    IO<Kakshya::DataVariant> valid_input(valid_variant);
    EXPECT_TRUE(extractor->validate_extraction_input(valid_input));

    std::vector<double> empty_data;
    Kakshya::DataVariant empty_variant { empty_data };
    IO<Kakshya::DataVariant> empty_input(empty_variant);

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
    Kakshya::DataVariant empty_variant { empty_data };

    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(empty_variant);
        EXPECT_TRUE(result.empty());
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

    Kakshya::DataVariant variant { problematic_data };
    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);

    EXPECT_NO_THROW({
        auto result = extractor->extract_data(variant);
    });
}

class FeatureExtractorPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        large_data.resize(44100);
        for (size_t i = 0; i < large_data.size(); ++i) {
            large_data[i] = std::sin(2.0 * M_PI * 440.0 * i / 44100.0);
        }

        extractor = std::make_shared<StandardFeatureExtractor>(1024, 512);
    }

    std::vector<double> large_data;
    std::shared_ptr<StandardFeatureExtractor> extractor;
};

TEST_F(FeatureExtractorPerformanceTest, LargeDataProcessing)
{
    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);
    extractor->set_parameter("overlap", 0.5);

    Kakshya::DataVariant audio_variant { large_data };

    auto start_time = std::chrono::high_resolution_clock::now();

    auto result = extractor->extract_data(audio_variant);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_FALSE(result.empty());
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(FeatureExtractorPerformanceTest, BatchProcessing)
{
    std::vector<Kakshya::DataVariant> batch_data;
    for (int i = 0; i < 5; ++i) {
        batch_data.emplace_back(large_data);
    }

    extractor->set_extraction_method(ExtractionMethod::OVERLAPPING_WINDOWS);

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::vector<double>> results;
    for (const auto& variant : batch_data) {
        results.push_back(extractor->extract_data(variant));
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
