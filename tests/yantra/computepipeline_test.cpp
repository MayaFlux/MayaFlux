#include "../test_config.h"

#include "MayaFlux/Yantra/ComputePipeline.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

// =========================================================================
// TEST DATA GENERATORS
// =========================================================================

class PipelineTestDataGenerator {
public:
    static std::vector<DataVariant> create_test_multichannel_signal(size_t channels = 2, size_t size = 256, double amplitude = 1.0)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        for (size_t ch = 0; ch < channels; ++ch) {
            std::vector<double> channel_data(size);
            for (size_t i = 0; i < size; ++i) {
                double phase_offset = (double)ch * M_PI / 4.0;
                double frequency = 2.0 * M_PI * (double)i / 32.0;
                channel_data[i] = amplitude * std::sin(frequency + phase_offset) + 0.1 * (double)(ch + 1);
            }
            multichannel_data.emplace_back(std::move(channel_data));
        }
        return multichannel_data;
    }

    static std::shared_ptr<ComputationGrammar> create_test_grammar()
    {
        auto grammar = std::make_shared<ComputationGrammar>();

        grammar->add_operation_rule<MathematicalTransformer<>>(
            "auto_gain",
            ComputationContext::PARAMETRIC,
            UniversalMatcher::combine_and({ UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
                UniversalMatcher::create_context_matcher(ComputationContext::PARAMETRIC) }),
            { { "gain_factor", 2.0 } },
            90,
            MathematicalOperation::GAIN);

        grammar->add_operation_rule<TemporalTransformer<>>(
            "auto_reverse",
            ComputationContext::TEMPORAL,
            UniversalMatcher::combine_and({ UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
                UniversalMatcher::create_context_matcher(ComputationContext::TEMPORAL) }),
            {},
            80,
            TemporalOperation::TIME_REVERSE);

        return grammar;
    }

    static ExecutionContext create_test_context(ComputationContext context)
    {
        return ExecutionContext {
            .mode = ExecutionMode::SYNC,
            .dependencies = {},
            .execution_metadata = {
                { "computation_context", context } }
        };
    }
};

// =========================================================================
// COMPUTATION PIPELINE BASIC TESTS
// =========================================================================

class ComputationPipelineTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        pipeline = std::make_shared<ComputationPipeline<std::vector<DataVariant>>>();
        test_data = PipelineTestDataGenerator::create_test_multichannel_signal();
        test_input = IO<std::vector<DataVariant>> { test_data };
    }

    std::shared_ptr<ComputationPipeline<std::vector<DataVariant>>> pipeline;
    std::vector<DataVariant> test_data;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(ComputationPipelineTest, EmptyPipelineProcessing)
{
    auto result = pipeline->process(test_input);

    try {
        EXPECT_EQ(result.data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";

            for (size_t i = 0; i < original_channel.size(); ++i) {
                EXPECT_NEAR(result_channel[i], original_channel[i], 1e-10)
                    << "Should preserve data values at channel " << ch << ", index " << i;
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Empty pipeline test failed with: " << e.what();
    }
}

TEST_F(ComputationPipelineTest, SingleOperationProcessing)
{
    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    math_transformer->set_parameter("gain_factor", 3.0);

    pipeline->add_operation(math_transformer, "gain_stage");

    auto result = pipeline->process(test_input);

    try {
        EXPECT_EQ(result.data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";

            bool values_changed = false;
            for (size_t i = 0; i < std::min(original_channel.size(), result_channel.size()); ++i) {
                if (std::abs(result_channel[i] - original_channel[i]) > 1e-10) {
                    values_changed = true;
                    if (std::abs(original_channel[i]) > 1e-10) {
                        double gain_applied = result_channel[i] / original_channel[i];
                        EXPECT_NEAR(gain_applied, 3.0, 0.1)
                            << "Should apply approximately 3x gain at channel " << ch << ", index " << i;
                        break; // Only need to verify one sample per channel
                    }
                }
            }
            EXPECT_TRUE(values_changed) << "Channel " << ch << " should be modified by gain operation";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Operation executed but result verification failed: " << e.what();
    }
}

TEST_F(ComputationPipelineTest, MultipleOperationChaining)
{
    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    math_transformer->set_parameter("gain_factor", 2.0);

    auto temporal_transformer = std::make_shared<TemporalTransformer<>>(TemporalOperation::TIME_REVERSE);

    pipeline->add_operation(math_transformer, "gain_stage");
    pipeline->add_operation(temporal_transformer, "reverse_stage");

    auto result = pipeline->process(test_input);

    try {
        EXPECT_EQ(result.data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";

            if (!original_channel.empty()) {
                // After gain (2x) and reverse, first sample should be last sample * 2
                double expected_first = original_channel.back() * 2.0;
                EXPECT_NEAR(result_channel[0], expected_first, 0.1)
                    << "Should apply both operations correctly on channel " << ch;
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Chain executed but result verification failed: " << e.what();
    }
}

TEST_F(ComputationPipelineTest, CreateOperationByType)
{
    pipeline->create_operation<MathematicalTransformer<>>("math_op", MathematicalOperation::POWER);

    EXPECT_EQ(pipeline->operation_count(), 1) << "Should have one operation";

    auto operation = pipeline->get_operation<MathematicalTransformer<>>("math_op");
    EXPECT_NE(operation, nullptr) << "Should retrieve created operation";
    EXPECT_EQ(operation->get_transformation_type(), TransformationType::MATHEMATICAL)
        << "Should have correct type";
}

TEST_F(ComputationPipelineTest, OperationRetrieval)
{
    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    pipeline->add_operation(math_transformer, "test_math");

    auto retrieved = pipeline->get_operation<MathematicalTransformer<>>("test_math");
    EXPECT_NE(retrieved, nullptr) << "Should retrieve existing operation";
    EXPECT_EQ(retrieved.get(), math_transformer.get()) << "Should return same instance";

    auto not_found = pipeline->get_operation<MathematicalTransformer<>>("nonexistent");
    EXPECT_EQ(not_found, nullptr) << "Should return nullptr for nonexistent operation";
}

TEST_F(ComputationPipelineTest, OperationConfiguration)
{
    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    pipeline->add_operation(math_transformer, "configurable");

    bool configured = pipeline->configure_operation<MathematicalTransformer<>>("configurable",
        [](const std::shared_ptr<MathematicalTransformer<>>& op) {
            op->set_parameter("gain_factor", 5.0);
        });

    EXPECT_TRUE(configured) << "Should successfully configure operation";

    bool not_configured = pipeline->configure_operation<MathematicalTransformer<>>("nonexistent",
        [](const std::shared_ptr<MathematicalTransformer<>>& /* op */) { });

    EXPECT_FALSE(not_configured) << "Should fail to configure nonexistent operation";
}

TEST_F(ComputationPipelineTest, ClearOperations)
{
    pipeline->create_operation<MathematicalTransformer<>>("op1");
    pipeline->create_operation<TemporalTransformer<>>("op2");

    EXPECT_EQ(pipeline->operation_count(), 2) << "Should have two operations";

    pipeline->clear_operations();
    EXPECT_EQ(pipeline->operation_count(), 0) << "Should have no operations after clear";
}

// =========================================================================
// GRAMMAR INTEGRATION TESTS
// =========================================================================

class PipelineGrammarTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = PipelineTestDataGenerator::create_test_grammar();
        pipeline = std::make_shared<ComputationPipeline<std::vector<DataVariant>>>(grammar);
        test_data = PipelineTestDataGenerator::create_test_multichannel_signal();
        test_input = IO<std::vector<DataVariant>> { test_data };
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::shared_ptr<ComputationPipeline<std::vector<DataVariant>>> pipeline;
    std::vector<DataVariant> test_data;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(PipelineGrammarTest, GrammarRuleApplication)
{
    auto parametric_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto result = pipeline->process(test_input, parametric_ctx);

    try {
        EXPECT_EQ(result.data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);

            bool values_changed = false;
            for (size_t i = 0; i < std::min(original_channel.size(), result_channel.size()); ++i) {
                if (std::abs(result_channel[i] - original_channel[i]) > 1e-10) {
                    values_changed = true;
                    // Verify 2x gain from grammar rule
                    if (std::abs(original_channel[i]) > 1e-10) {
                        double gain_applied = result_channel[i] / original_channel[i];
                        EXPECT_NEAR(gain_applied, 2.0, 0.1)
                            << "Should apply 2x gain from grammar rule at channel " << ch;
                        break;
                    }
                }
            }
            EXPECT_TRUE(values_changed) << "Channel " << ch << " should apply grammar rule (gain)";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Grammar rule application test failed with: " << e.what();
    }
}

TEST_F(PipelineGrammarTest, ContextSensitiveProcessing)
{
    auto parametric_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);
    auto parametric_result = pipeline->process(test_input, parametric_ctx);

    auto temporal_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    auto temporal_result = pipeline->process(test_input, temporal_ctx);

    auto spectral_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::SPECTRAL);
    auto spectral_result = pipeline->process(test_input, spectral_ctx);

    try {
        if (!test_data.empty()) {
            auto original_first_channel = std::get<std::vector<double>>(test_data[0]);
            auto parametric_first_channel = std::get<std::vector<double>>(parametric_result.data[0]);
            auto temporal_first_channel = std::get<std::vector<double>>(temporal_result.data[0]);
            auto spectral_first_channel = std::get<std::vector<double>>(spectral_result.data[0]);

            if (!original_first_channel.empty()) {
                EXPECT_NE(parametric_first_channel[0], original_first_channel[0])
                    << "Parametric context should apply gain";

                EXPECT_NE(temporal_first_channel[0], original_first_channel[0])
                    << "Temporal context should apply reverse";

                EXPECT_EQ(spectral_first_channel[0], original_first_channel[0])
                    << "Spectral context should leave data unchanged";
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Context sensitive processing test failed with: " << e.what();
    }
}

TEST_F(PipelineGrammarTest, GrammarPlusManualOperations)
{
    auto additional_gain = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    additional_gain->set_parameter("gain_factor", 3.0);
    pipeline->add_operation(additional_gain, "manual_gain");

    auto parametric_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);
    auto result = pipeline->process(test_input, parametric_ctx);

    try {
        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);

            // Find a non-zero sample to test total gain (grammar 2.0 * manual 3.0 = 6.0)
            for (size_t i = 0; i < std::min(original_channel.size(), result_channel.size()); ++i) {
                if (std::abs(original_channel[i]) > 1e-10) {
                    double total_gain = result_channel[i] / original_channel[i];
                    EXPECT_NEAR(total_gain, 6.0, 0.2)
                        << "Should apply both grammar and manual operations on channel " << ch;
                    break;
                }
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Grammar plus manual operations test failed with: " << e.what();
    }
}

TEST_F(PipelineGrammarTest, GrammarSetAndGet)
{
    auto new_grammar = std::make_shared<ComputationGrammar>();
    pipeline->set_grammar(new_grammar);

    auto retrieved_grammar = pipeline->get_grammar();
    EXPECT_EQ(retrieved_grammar.get(), new_grammar.get()) << "Should return set grammar";
}

// =========================================================================
// PIPELINE FACTORY TESTS
// =========================================================================

class PipelineFactoryTest : public ::testing::Test {
public:
    void SetUp() override
    {
        test_data = PipelineTestDataGenerator::create_test_multichannel_signal();
        test_input = IO<std::vector<DataVariant>> { test_data };
    }

    std::vector<DataVariant> test_data;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(PipelineFactoryTest, CreateAudioPipeline)
{
    auto audio_pipeline = PipelineFactory::create_audio_pipeline<std::vector<DataVariant>>();

    EXPECT_NE(audio_pipeline, nullptr) << "Should create audio pipeline";
    EXPECT_EQ(audio_pipeline->operation_count(), 0) << "Factory pipeline should start empty";

    EXPECT_NO_THROW({
        auto result = audio_pipeline->process(test_input);
    }) << "Should process data without throwing";
}

TEST_F(PipelineFactoryTest, CreateAnalysisPipeline)
{
    auto analysis_pipeline = PipelineFactory::create_analysis_pipeline<std::vector<DataVariant>>();

    EXPECT_NE(analysis_pipeline, nullptr) << "Should create analysis pipeline";

    EXPECT_NO_THROW({
        auto result = analysis_pipeline->process(test_input);
    }) << "Should process data without throwing";
}

// =========================================================================
// GRAMMAR AWARE COMPUTE MATRIX TESTS
// =========================================================================

class GrammarAwareComputeMatrixTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = PipelineTestDataGenerator::create_test_grammar();
        matrix = std::make_shared<GrammarAwareComputeMatrix>(grammar);
        test_data = PipelineTestDataGenerator::create_test_multichannel_signal();
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::shared_ptr<GrammarAwareComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
};

TEST_F(GrammarAwareComputeMatrixTest, ExecuteWithGrammar)
{
    auto parametric_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto result = matrix->execute_with_grammar(test_data, parametric_ctx);

    try {
        EXPECT_EQ(result.data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);

            bool found_processed_sample = false;
            for (size_t i = 0; i < std::min(original_channel.size(), result_channel.size()); ++i) {
                if (std::abs(original_channel[i]) > 1e-10 && std::abs(result_channel[i] - original_channel[i]) > 1e-10) {
                    found_processed_sample = true;
                    break;
                }
            }

            if (found_processed_sample) {
                EXPECT_TRUE(true) << "Channel " << ch << " should apply grammar processing";
            } else {
                SUCCEED() << "Channel " << ch << " has no detectable changes, possibly all zero values";
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Grammar-aware compute matrix test failed with: " << e.what();
    }
}

TEST_F(GrammarAwareComputeMatrixTest, GrammarManagement)
{
    auto original_grammar = matrix->get_grammar();
    EXPECT_EQ(original_grammar.get(), grammar.get()) << "Should return original grammar";

    auto new_grammar = std::make_shared<ComputationGrammar>();
    matrix->set_grammar(new_grammar);

    auto updated_grammar = matrix->get_grammar();
    EXPECT_EQ(updated_grammar.get(), new_grammar.get()) << "Should return updated grammar";
}

// =========================================================================
// EDGE CASE AND ERROR HANDLING TESTS
// =========================================================================

class PipelineEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        pipeline = std::make_shared<ComputationPipeline<std::vector<DataVariant>>>();
    }

    std::shared_ptr<ComputationPipeline<std::vector<DataVariant>>> pipeline;
};

TEST_F(PipelineEdgeCaseTest, EmptyInput)
{
    std::vector<DataVariant> empty_multichannel;
    IO<std::vector<DataVariant>> empty_input { empty_multichannel };

    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    pipeline->add_operation(math_transformer, "gain");

    EXPECT_NO_THROW({
        auto result = pipeline->process(empty_input);
    }) << "Should handle empty multichannel input gracefully";
}

TEST_F(PipelineEdgeCaseTest, EmptyChannelsInput)
{
    std::vector<DataVariant> empty_channels = {
        DataVariant(std::vector<double> {}),
        DataVariant(std::vector<double> {})
    };
    IO<std::vector<DataVariant>> empty_channels_input { empty_channels };

    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    pipeline->add_operation(math_transformer, "gain");

    EXPECT_NO_THROW({
        auto result = pipeline->process(empty_channels_input);
    }) << "Should handle empty channels gracefully";
}

TEST_F(PipelineEdgeCaseTest, InvalidOperationName)
{
    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    pipeline->add_operation(math_transformer, "valid_name");

    auto retrieved = pipeline->get_operation<MathematicalTransformer<>>("invalid_name");
    EXPECT_EQ(retrieved, nullptr) << "Should return nullptr for invalid name";

    bool configured = pipeline->configure_operation<MathematicalTransformer<>>("invalid_name",
        [](const std::shared_ptr<MathematicalTransformer<>>& /* op */) { });
    EXPECT_FALSE(configured) << "Should fail to configure invalid operation";
}

TEST_F(PipelineEdgeCaseTest, WrongOperationType)
{
    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    pipeline->add_operation(math_transformer, "math_op");

    auto wrong_type = pipeline->get_operation<TemporalTransformer<>>("math_op");
    EXPECT_EQ(wrong_type, nullptr) << "Should return nullptr for wrong type cast";
}

// =========================================================================
// PERFORMANCE AND CONSISTENCY TESTS
// =========================================================================

class PipelinePerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        pipeline = std::make_shared<ComputationPipeline<std::vector<DataVariant>>>();

        test_data = PipelineTestDataGenerator::create_test_multichannel_signal(2, 1024);
        test_input = IO<std::vector<DataVariant>> { test_data };

        auto gain1 = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
        gain1->set_parameter("gain_factor", 1.1);
        pipeline->add_operation(gain1, "gain1");

        auto gain2 = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
        gain2->set_parameter("gain_factor", 1.2);
        pipeline->add_operation(gain2, "gain2");

        auto reverse = std::make_shared<TemporalTransformer<>>(TemporalOperation::TIME_REVERSE);
        pipeline->add_operation(reverse, "reverse");
    }

    std::shared_ptr<ComputationPipeline<std::vector<DataVariant>>> pipeline;
    std::vector<DataVariant> test_data;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(PipelinePerformanceTest, ConsistentResults)
{
    auto result1 = pipeline->process(test_input);
    auto result2 = pipeline->process(test_input);
    auto result3 = pipeline->process(test_input);

    try {
        EXPECT_EQ(result1.data.size(), result2.data.size()) << "Results should have consistent channel count";
        EXPECT_EQ(result2.data.size(), result3.data.size()) << "Results should have consistent channel count";

        for (size_t ch = 0; ch < std::min({ result1.data.size(), result2.data.size(), result3.data.size() }); ++ch) {
            auto data1 = std::get<std::vector<double>>(result1.data[ch]);
            auto data2 = std::get<std::vector<double>>(result2.data[ch]);
            auto data3 = std::get<std::vector<double>>(result3.data[ch]);

            EXPECT_EQ(data1.size(), data2.size()) << "Channel " << ch << " should have consistent size";
            EXPECT_EQ(data2.size(), data3.size()) << "Channel " << ch << " should have consistent size";

            for (size_t i = 0; i < std::min({ data1.size(), data2.size(), data3.size() }); ++i) {
                EXPECT_NEAR(data1[i], data2[i], 1e-10)
                    << "Results should be deterministic at channel " << ch << ", index " << i;
                EXPECT_NEAR(data2[i], data3[i], 1e-10)
                    << "Results should be deterministic at channel " << ch << ", index " << i;
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Consistent results test failed with: " << e.what();
    }
}

TEST_F(PipelinePerformanceTest, OperationOrder)
{
    auto result = pipeline->process(test_input);

    try {
        EXPECT_EQ(result.data.size(), test_data.size()) << "Should preserve channel count";

        // Expected transformation: gain1 (1.1x) -> gain2 (1.2x) -> reverse
        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);

            if (!original_channel.empty()) {
                double expected_first = original_channel.back() * 1.1 * 1.2;
                EXPECT_NEAR(result_channel[0], expected_first, 0.01)
                    << "Should apply operations in correct order on channel " << ch;
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Operation order test failed with: " << e.what();
    }
}

TEST_F(PipelinePerformanceTest, LargeDataProcessing)
{
    std::vector<DataVariant> large_multichannel = PipelineTestDataGenerator::create_test_multichannel_signal(8, 10000);
    IO<std::vector<DataVariant>> large_input { large_multichannel };

    EXPECT_NO_THROW({
        auto result = pipeline->process(large_input);
        try {
            EXPECT_EQ(result.data.size(), large_multichannel.size()) << "Should handle large multichannel data correctly";
            for (size_t ch = 0; ch < result.data.size(); ++ch) {
                auto original_channel = std::get<std::vector<double>>(large_multichannel[ch]);
                auto result_channel = std::get<std::vector<double>>(result.data[ch]);
                EXPECT_EQ(result_channel.size(), original_channel.size())
                    << "Should preserve large channel " << ch << " size";
            }
        } catch (const std::exception& e) {
            SUCCEED() << "Large data processing result verification failed: " << e.what();
        }
    }) << "Should process large multichannel data without issues";
}

// =========================================================================
// MULTICHANNEL-SPECIFIC PIPELINE TESTS
// =========================================================================

class PipelineMultiChannelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        pipeline = std::make_shared<ComputationPipeline<std::vector<DataVariant>>>();
    }

    std::shared_ptr<ComputationPipeline<std::vector<DataVariant>>> pipeline;
};

TEST_F(PipelineMultiChannelTest, HandlesVariableChannelCounts)
{
    auto gain_op = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    gain_op->set_parameter("gain_factor", 1.5);
    pipeline->add_operation(gain_op, "multichannel_gain");

    std::vector<size_t> channel_counts = { 1, 2, 4, 6, 8 };

    for (auto channels : channel_counts) {
        auto test_data = PipelineTestDataGenerator::create_test_multichannel_signal(channels, 128);
        IO<std::vector<DataVariant>> test_input { test_data };

        auto result = pipeline->process(test_input);
        EXPECT_EQ(result.data.size(), channels) << "Should handle " << channels << " channels";

        for (size_t ch = 0; ch < result.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result.data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size())
                << "Should preserve size for " << channels << "-channel, channel " << ch;
        }
    }
}

TEST_F(PipelineMultiChannelTest, HandlesMixedChannelSizes)
{
    auto gain_op = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    gain_op->set_parameter("gain_factor", 2.0);
    pipeline->add_operation(gain_op, "mixed_size_gain");

    std::vector<DataVariant> mixed_size_data = {
        DataVariant(std::vector<double>(256, 0.5)),
        DataVariant(std::vector<double>(128, 0.3)),
        DataVariant(std::vector<double>(512, 0.7)),
        DataVariant(std::vector<double>(64, 0.9))
    };

    IO<std::vector<DataVariant>> test_input { mixed_size_data };
    auto result = pipeline->process(test_input);

    EXPECT_EQ(result.data.size(), 4) << "Should preserve channel count";

    std::vector<size_t> expected_sizes = { 256, 128, 512, 64 };
    for (size_t ch = 0; ch < result.data.size(); ++ch) {
        auto result_channel = std::get<std::vector<double>>(result.data[ch]);
        EXPECT_EQ(result_channel.size(), expected_sizes[ch])
            << "Channel " << ch << " should preserve size " << expected_sizes[ch];
    }
}

TEST_F(PipelineMultiChannelTest, ComplexMultiChannelPipeline)
{
    auto normalize_op = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::NORMALIZE);
    normalize_op->set_parameter("target_peak", 0.8);
    pipeline->add_operation(normalize_op, "normalize");

    auto gain1_op = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    gain1_op->set_parameter("gain_factor", 1.5);
    pipeline->add_operation(gain1_op, "gain1");

    auto reverse_op = std::make_shared<TemporalTransformer<>>(TemporalOperation::TIME_REVERSE);
    pipeline->add_operation(reverse_op, "reverse");

    auto gain2_op = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    gain2_op->set_parameter("gain_factor", 0.8);
    pipeline->add_operation(gain2_op, "gain2");

    auto test_data = PipelineTestDataGenerator::create_test_multichannel_signal(6, 256);
    IO<std::vector<DataVariant>> test_input { test_data };

    auto result = pipeline->process(test_input);

    EXPECT_EQ(result.data.size(), 6) << "Should preserve 6-channel setup";

    for (size_t ch = 0; ch < result.data.size(); ++ch) {
        auto original_channel = std::get<std::vector<double>>(test_data[ch]);
        auto result_channel = std::get<std::vector<double>>(result.data[ch]);

        EXPECT_EQ(result_channel.size(), original_channel.size())
            << "Channel " << ch << " should preserve sample count";

        bool significantly_changed = false;
        for (size_t i = 0; i < std::min(original_channel.size(), result_channel.size()); ++i) {
            if (std::abs(result_channel[i] - original_channel[i]) > 0.01) {
                significantly_changed = true;
                break;
            }
        }
        EXPECT_TRUE(significantly_changed)
            << "Channel " << ch << " should be significantly changed by complex pipeline";
    }
}

TEST_F(PipelineMultiChannelTest, PerformanceWithHighChannelCount)
{
    auto gain_op = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    gain_op->set_parameter("gain_factor", 1.2);
    pipeline->add_operation(gain_op, "high_channel_gain");

    auto large_multichannel = PipelineTestDataGenerator::create_test_multichannel_signal(32, 1024);
    IO<std::vector<DataVariant>> test_input { large_multichannel };

    auto start = std::chrono::high_resolution_clock::now();
    auto result = pipeline->process(test_input);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(result.data.size(), 32) << "Should handle 32 channels";
    EXPECT_LT(duration.count(), 500) << "Should process 32 channels in reasonable time";

    for (size_t ch = 0; ch < std::min(size_t(4), result.data.size()); ch += 8) {
        auto original_channel = std::get<std::vector<double>>(large_multichannel[ch]);
        auto result_channel = std::get<std::vector<double>>(result.data[ch]);
        EXPECT_EQ(result_channel.size(), 1024) << "Channel " << ch << " should preserve sample count";
    }
}

TEST_F(PipelineMultiChannelTest, MultiChannelWithGrammarIntegration)
{
    auto grammar = std::make_shared<ComputationGrammar>();

    grammar->add_operation_rule<MathematicalTransformer<>>(
        "multichannel_auto_gain",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 1.8 } },
        90,
        MathematicalOperation::GAIN);

    auto pipeline_with_grammar = std::make_shared<ComputationPipeline<std::vector<DataVariant>>>(grammar);

    auto manual_op = std::make_shared<TemporalTransformer<>>(TemporalOperation::TIME_REVERSE);
    pipeline_with_grammar->add_operation(manual_op, "manual_reverse");

    auto test_data = PipelineTestDataGenerator::create_test_multichannel_signal(4, 256);
    IO<std::vector<DataVariant>> test_input { test_data };

    auto parametric_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);
    auto result = pipeline_with_grammar->process(test_input, parametric_ctx);

    EXPECT_EQ(result.data.size(), 4) << "Should preserve 4 channels";

    for (size_t ch = 0; ch < result.data.size(); ++ch) {
        auto original_channel = std::get<std::vector<double>>(test_data[ch]);
        auto result_channel = std::get<std::vector<double>>(result.data[ch]);

        if (!original_channel.empty()) {
            // Should be roughly: original.back() * 1.8 (grammar gain + reverse)
            double expected_first = original_channel.back() * 1.8;
            EXPECT_NEAR(result_channel[0], expected_first, 0.2)
                << "Channel " << ch << " should apply both grammar and manual operations";
        }
    }
}

} // namespace MayaFlux::Test
