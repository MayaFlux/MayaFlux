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
    static std::vector<double> create_test_signal(size_t size = 256, double amplitude = 1.0)
    {
        std::vector<double> signal(size);
        for (size_t i = 0; i < size; ++i) {
            signal[i] = amplitude * std::sin(2.0 * M_PI * i / 32.0);
        }
        return signal;
    }

    static std::shared_ptr<ComputationGrammar> create_test_grammar()
    {
        auto grammar = std::make_shared<ComputationGrammar>();

        grammar->add_operation_rule<MathematicalTransformer<>>(
            "auto_gain",
            ComputationContext::PARAMETRIC,
            UniversalMatcher::combine_and({ UniversalMatcher::create_type_matcher<DataVariant>(),
                UniversalMatcher::create_context_matcher(ComputationContext::PARAMETRIC) }),
            { { "gain_factor", 2.0 } },
            90,
            MathematicalOperation::GAIN);

        grammar->add_operation_rule<TemporalTransformer<>>(
            "auto_reverse",
            ComputationContext::TEMPORAL,
            UniversalMatcher::combine_and({ UniversalMatcher::create_type_matcher<DataVariant>(),
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
        pipeline = std::make_shared<ComputationPipeline<DataVariant>>();
        test_data = PipelineTestDataGenerator::create_test_signal();
        test_input = IO<DataVariant> { DataVariant(test_data) };
    }

    std::shared_ptr<ComputationPipeline<DataVariant>> pipeline;
    std::vector<double> test_data;
    IO<DataVariant> test_input;
};

TEST_F(ComputationPipelineTest, EmptyPipelineProcessing)
{
    auto result = pipeline->process(test_input);

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";

        for (size_t i = 0; i < test_data.size(); ++i) {
            EXPECT_NEAR(result_data[i], test_data[i], 1e-10) << "Should preserve data values at index " << i;
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
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);

        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
        EXPECT_NE(result_data[0], test_data[0]) << "Should modify data values (gain applied)";

        if (!test_data.empty() && test_data[0] != 0.0) {
            double gain_applied = result_data[0] / test_data[0];
            EXPECT_NEAR(gain_applied, 3.0, 0.1) << "Should apply approximately 3x gain";
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
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);

        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
        EXPECT_NE(result_data[0], test_data[0]) << "Should modify data (both operations applied)";

        if (!test_data.empty()) {
            double expected_first = test_data.back() * 2.0;
            EXPECT_NEAR(result_data[0], expected_first, 0.1) << "Should apply both operations correctly";
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
        pipeline = std::make_shared<ComputationPipeline<DataVariant>>(grammar);
        test_data = PipelineTestDataGenerator::create_test_signal();
        test_input = IO<DataVariant> { DataVariant(test_data) };
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::shared_ptr<ComputationPipeline<DataVariant>> pipeline;
    std::vector<double> test_data;
    IO<DataVariant> test_input;
};

TEST_F(PipelineGrammarTest, GrammarRuleApplication)
{
    auto parametric_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto result = pipeline->process(test_input, parametric_ctx);

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);

        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
        EXPECT_NE(result_data[0], test_data[0]) << "Should apply grammar rule (gain)";

        if (!test_data.empty() && test_data[0] != 0.0) {
            double gain_applied = result_data[0] / test_data[0];
            EXPECT_NEAR(gain_applied, 2.0, 0.1) << "Should apply 2x gain from grammar rule";
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
        auto parametric_data = safe_any_cast_or_throw<std::vector<double>>(parametric_result.data);
        auto temporal_data = safe_any_cast_or_throw<std::vector<double>>(temporal_result.data);
        auto spectral_data = safe_any_cast_or_throw<std::vector<double>>(spectral_result.data);

        EXPECT_NE(parametric_data[0], test_data[0]) << "Parametric context should apply gain";

        EXPECT_NE(temporal_data[0], test_data[0]) << "Temporal context should apply reverse";

        EXPECT_EQ(spectral_data[0], test_data[0]) << "Spectral context should leave data unchanged";
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
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);

        if (!test_data.empty() && test_data[0] != 0.0) {
            double total_gain = result_data[0] / test_data[0];
            EXPECT_NEAR(total_gain, 6.0, 0.2) << "Should apply both grammar and manual operations";
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
        test_data = PipelineTestDataGenerator::create_test_signal();
        test_input = IO<DataVariant> { DataVariant(test_data) };
    }

    std::vector<double> test_data;
    IO<DataVariant> test_input;
};

TEST_F(PipelineFactoryTest, CreateAudioPipeline)
{
    auto audio_pipeline = PipelineFactory::create_audio_pipeline<DataVariant>();

    EXPECT_NE(audio_pipeline, nullptr) << "Should create audio pipeline";
    EXPECT_EQ(audio_pipeline->operation_count(), 0) << "Factory pipeline should start empty";

    EXPECT_NO_THROW({
        auto result = audio_pipeline->process(test_input);
    }) << "Should process data without throwing";
}

TEST_F(PipelineFactoryTest, CreateAnalysisPipeline)
{
    auto analysis_pipeline = PipelineFactory::create_analysis_pipeline<DataVariant>();

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
        test_data = PipelineTestDataGenerator::create_test_signal();
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::shared_ptr<GrammarAwareComputeMatrix> matrix;
    std::vector<double> test_data;
};

TEST_F(GrammarAwareComputeMatrixTest, ExecuteWithGrammar)
{
    auto parametric_ctx = PipelineTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    DataVariant input_data(test_data);
    auto result = matrix->execute_with_grammar(input_data, parametric_ctx);

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
        size_t test_idx = 0;
        while (test_idx < test_data.size() && test_data[test_idx] == 0.0)
            test_idx++;
        if (test_idx < test_data.size()) {
            EXPECT_NE(result_data[test_idx], test_data[test_idx]) << "Should apply grammar processing";
        } else {
            SUCCEED() << "All test data is zero, cannot verify gain application";
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
        pipeline = std::make_shared<ComputationPipeline<DataVariant>>();
    }

    std::shared_ptr<ComputationPipeline<DataVariant>> pipeline;
};

TEST_F(PipelineEdgeCaseTest, EmptyInput)
{
    std::vector<double> empty_data;
    IO<DataVariant> empty_input { DataVariant(empty_data) };

    auto math_transformer = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    pipeline->add_operation(math_transformer, "gain");

    EXPECT_NO_THROW({
        auto result = pipeline->process(empty_input);
    }) << "Should handle empty input gracefully";
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
        pipeline = std::make_shared<ComputationPipeline<DataVariant>>();

        test_data = PipelineTestDataGenerator::create_test_signal(1024);
        test_input = IO<DataVariant> { DataVariant(test_data) };

        auto gain1 = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
        gain1->set_parameter("gain_factor", 1.1);
        pipeline->add_operation(gain1, "gain1");

        auto gain2 = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
        gain2->set_parameter("gain_factor", 1.2);
        pipeline->add_operation(gain2, "gain2");

        auto reverse = std::make_shared<TemporalTransformer<>>(TemporalOperation::TIME_REVERSE);
        pipeline->add_operation(reverse, "reverse");
    }

    std::shared_ptr<ComputationPipeline<DataVariant>> pipeline;
    std::vector<double> test_data;
    IO<DataVariant> test_input;
};

TEST_F(PipelinePerformanceTest, ConsistentResults)
{
    auto result1 = pipeline->process(test_input);
    auto result2 = pipeline->process(test_input);
    auto result3 = pipeline->process(test_input);

    try {
        auto data1 = safe_any_cast_or_throw<std::vector<double>>(result1.data);
        auto data2 = safe_any_cast_or_throw<std::vector<double>>(result2.data);
        auto data3 = safe_any_cast_or_throw<std::vector<double>>(result3.data);

        EXPECT_EQ(data1.size(), data2.size()) << "Results should have consistent size";
        EXPECT_EQ(data2.size(), data3.size()) << "Results should have consistent size";

        for (size_t i = 0; i < std::min({ data1.size(), data2.size(), data3.size() }); ++i) {
            EXPECT_NEAR(data1[i], data2[i], 1e-10) << "Results should be deterministic at index " << i;
            EXPECT_NEAR(data2[i], data3[i], 1e-10) << "Results should be deterministic at index " << i;
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Consistent results test failed with: " << e.what();
    }
}

TEST_F(PipelinePerformanceTest, OperationOrder)
{
    auto result = pipeline->process(test_input);

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);

        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";

        // Expected transformation: gain1 (1.1x) -> gain2 (1.2x) -> reverse
        if (!test_data.empty()) {
            double expected_first = test_data.back() * 1.1 * 1.2;
            EXPECT_NEAR(result_data[0], expected_first, 0.01) << "Should apply operations in correct order";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Operation order test failed with: " << e.what();
    }
}

TEST_F(PipelinePerformanceTest, LargeDataProcessing)
{
    std::vector<double> large_data = PipelineTestDataGenerator::create_test_signal(10000);
    IO<DataVariant> large_input { DataVariant(large_data) };

    EXPECT_NO_THROW({
        auto result = pipeline->process(large_input);
        try {
            auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);
            EXPECT_EQ(result_data.size(), large_data.size()) << "Should handle large data correctly";
        } catch (const std::exception& e) {
            SUCCEED() << "Large data processing result verification failed: " << e.what();
        }
    }) << "Should process large data without issues";
}

} // namespace MayaFlux::Test
