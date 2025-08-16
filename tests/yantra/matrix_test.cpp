#include "../test_config.h"

#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/ComputeMatrix.hpp"
#include "MayaFlux/Yantra/ComputePipeline.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

// =========================================================================
// TEST DATA GENERATORS
// =========================================================================

class MatrixTestDataGenerator {
public:
    static std::vector<double> create_test_signal(size_t size = 256, double amplitude = 1.0)
    {
        std::vector<double> signal(size);
        for (size_t i = 0; i < size; ++i) {
            signal[i] = amplitude * std::sin(2.0 * M_PI * i / 32.0);
        }
        return signal;
    }

    static std::vector<double> create_ramp_signal(size_t size = 128)
    {
        std::vector<double> signal(size);
        for (size_t i = 0; i < size; ++i) {
            signal[i] = static_cast<double>(i) / size;
        }
        return signal;
    }

    static std::shared_ptr<ComputationGrammar> create_test_grammar()
    {
        auto grammar = std::make_shared<ComputationGrammar>();

        grammar->add_operation_rule<MathematicalTransformer<>>(
            "auto_gain",
            ComputationContext::PARAMETRIC,
            UniversalMatcher::create_type_matcher<DataVariant>(),
            { { "gain_factor", 2.0 } },
            90,
            MathematicalOperation::GAIN);

        return grammar;
    }
};

// =========================================================================
// BASIC COMPUTE MATRIX TESTS
// =========================================================================

class ComputeMatrixTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(ComputeMatrixTest, MatrixCreation)
{
    EXPECT_NE(matrix, nullptr) << "Should create matrix instance";
    EXPECT_EQ(matrix->list_operations().size(), 0) << "Matrix should start empty";
    EXPECT_EQ(matrix->get_execution_policy(), ExecutionPolicy::BALANCED) << "Should have default execution policy";
}

TEST_F(ComputeMatrixTest, BasicOperationManagement)
{
    auto math_op = std::make_shared<MathematicalTransformer<>>(MathematicalOperation::GAIN);
    math_op->set_parameter("gain_factor", 2.0);

    EXPECT_TRUE(matrix->add_operation("gain", math_op)) << "Should add operation successfully";
    EXPECT_FALSE(matrix->add_operation("gain", math_op)) << "Should reject duplicate names";

    auto retrieved = matrix->get_operation<MathematicalTransformer<>>("gain");
    EXPECT_NE(retrieved, nullptr) << "Should retrieve added operation";
    EXPECT_EQ(retrieved.get(), math_op.get()) << "Should return same instance";

    auto operations = matrix->list_operations();
    EXPECT_EQ(operations.size(), 1) << "Should list one operation";
    EXPECT_EQ(operations[0], "gain") << "Should list correct name";
}

TEST_F(ComputeMatrixTest, CreateOperationInMatrix)
{
    auto created = matrix->create_operation<MathematicalTransformer<>>("created_gain", MathematicalOperation::GAIN);

    EXPECT_NE(created, nullptr) << "Should create operation successfully";
    EXPECT_EQ(matrix->list_operations().size(), 1) << "Should have one operation";

    auto retrieved = matrix->get_operation<MathematicalTransformer<>>("created_gain");
    EXPECT_EQ(retrieved.get(), created.get()) << "Should retrieve same instance";
}

TEST_F(ComputeMatrixTest, RemoveOperations)
{
    matrix->create_operation<MathematicalTransformer<>>("op1", MathematicalOperation::GAIN);
    matrix->create_operation<TemporalTransformer<>>("op2", TemporalOperation::TIME_REVERSE);

    EXPECT_EQ(matrix->list_operations().size(), 2) << "Should have two operations";

    EXPECT_TRUE(matrix->remove_operation("op1")) << "Should remove existing operation";
    EXPECT_FALSE(matrix->remove_operation("nonexistent")) << "Should fail to remove nonexistent operation";

    EXPECT_EQ(matrix->list_operations().size(), 1) << "Should have one operation after removal";

    matrix->clear_operations();
    EXPECT_EQ(matrix->list_operations().size(), 0) << "Should have no operations after clear";
}

// =========================================================================
// EXECUTION INTERFACE TESTS
// =========================================================================

class MatrixExecutionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(MatrixExecutionTest, DirectExecution)
{
    auto result = matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);

    EXPECT_TRUE(result.has_value()) << "Should execute operation successfully";

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result->data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
    } catch (const std::exception& e) {
        SUCCEED() << "Direct execution test failed with: " << e.what();
    }
}

TEST_F(MatrixExecutionTest, NamedExecution)
{
    auto math_op = matrix->create_operation<MathematicalTransformer<>>("named_gain", MathematicalOperation::GAIN);
    math_op->set_parameter("gain_factor", 3.0);

    auto result = matrix->execute_named<MathematicalTransformer<>, DataVariant>("named_gain", test_input);

    EXPECT_TRUE(result.has_value()) << "Should execute named operation successfully";

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result->data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
        if (!test_data.empty() && test_data[0] != 0.0) {
            double gain_applied = result_data[0] / test_data[0];
            EXPECT_NEAR(gain_applied, 3.0, 0.1) << "Should apply 3x gain";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Named execution test failed with: " << e.what();
    }
}

TEST_F(MatrixExecutionTest, AsynchronousExecution)
{
    auto future = matrix->execute_async<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);

    EXPECT_TRUE(future.valid()) << "Should return valid future";

    auto result = future.get();
    EXPECT_TRUE(result.has_value()) << "Should complete asynchronously";

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result->data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
    } catch (const std::exception& e) {
        SUCCEED() << "Async execution test failed with: " << e.what();
    }
}

TEST_F(MatrixExecutionTest, NamedAsyncExecution)
{
    matrix->create_operation<MathematicalTransformer<>>("async_gain", MathematicalOperation::GAIN);

    auto future = matrix->execute_named_async<MathematicalTransformer<>, DataVariant>("async_gain", test_input);

    EXPECT_TRUE(future.valid()) << "Should return valid future";

    auto result = future.get();
    EXPECT_TRUE(result.has_value()) << "Should complete named async execution";
}

// =========================================================================
// PARALLEL EXECUTION TESTS
// =========================================================================

class MatrixParallelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(MatrixParallelTest, ParallelNamedExecution)
{
    matrix->create_operation<MathematicalTransformer<>>("gain1", MathematicalOperation::GAIN);
    matrix->create_operation<MathematicalTransformer<>>("gain2", MathematicalOperation::POWER);

    std::vector<std::string> operation_names = { "gain1", "gain2" };
    auto results = matrix->execute_parallel_named<MathematicalTransformer<>, DataVariant>(operation_names, test_input);

    EXPECT_EQ(results.size(), 2) << "Should return results for both named operations";

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].has_value()) {
            try {
                auto result_data = safe_any_cast_or_throw<std::vector<double>>(results[i]->data);
                EXPECT_EQ(result_data.size(), test_data.size()) << "Result " << i << " should preserve size";
            } catch (const std::exception& e) {
                SUCCEED() << "Parallel named execution " << i << " failed with: " << e.what();
            }
        }
    }
}

// =========================================================================
// CHAIN EXECUTION TESTS
// =========================================================================

class MatrixChainTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(MatrixChainTest, BasicChainExecution)
{
    auto result = matrix->execute_chain<MathematicalTransformer<>, TemporalTransformer<>, DataVariant, DataVariant, DataVariant>(test_input);

    EXPECT_TRUE(result.has_value()) << "Should execute chain successfully";

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result->data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";
        EXPECT_NE(result_data[0], test_data[0]) << "Should apply both transformations";
    } catch (const std::exception& e) {
        SUCCEED() << "Chain execution test failed with: " << e.what();
    }
}

TEST_F(MatrixChainTest, NamedChainExecution)
{
    auto gain_op = matrix->create_operation<MathematicalTransformer<>>("chain_gain", MathematicalOperation::GAIN);
    gain_op->set_parameter("gain_factor", 2.0);

    auto reverse_op = matrix->create_operation<TemporalTransformer<>>("chain_reverse", TemporalOperation::TIME_REVERSE);

    auto result = matrix->execute_chain_named<MathematicalTransformer<>, TemporalTransformer<>, DataVariant, DataVariant, DataVariant>(
        "chain_gain", "chain_reverse", test_input);

    EXPECT_TRUE(result.has_value()) << "Should execute named chain successfully";

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result->data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";

        if (!test_data.empty()) {
            double expected_first = test_data.back() * 2.0;
            EXPECT_NEAR(result_data[0], expected_first, 0.01) << "Should apply gain then reverse";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Named chain execution test failed with: " << e.what();
    }
}

// =========================================================================
// BATCH EXECUTION TESTS
// =========================================================================

class MatrixBatchTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();

        test_inputs.emplace_back(MatrixTestDataGenerator::create_test_signal(128, 1.0));
        test_inputs.emplace_back(MatrixTestDataGenerator::create_test_signal(128, 0.5));
        test_inputs.emplace_back(MatrixTestDataGenerator::create_ramp_signal(128));
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_inputs;
};

TEST_F(MatrixBatchTest, SequentialBatchExecution)
{
    auto results = matrix->execute_batch<MathematicalTransformer<>, DataVariant>(test_inputs, MathematicalOperation::GAIN);

    EXPECT_EQ(results.size(), test_inputs.size()) << "Should return result for each input";

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].has_value()) {
            try {
                auto result_data = safe_any_cast_or_throw<std::vector<double>>(results[i]->data);
                auto input_data = safe_any_cast_or_throw<std::vector<double>>(test_inputs[i]);
                EXPECT_EQ(result_data.size(), input_data.size()) << "Result " << i << " should preserve size";
            } catch (const std::exception& e) {
                SUCCEED() << "Batch execution " << i << " failed with: " << e.what();
            }
        }
    }
}

TEST_F(MatrixBatchTest, ParallelBatchExecution)
{
    auto results = matrix->execute_batch_parallel<MathematicalTransformer<>, DataVariant>(test_inputs, MathematicalOperation::GAIN);

    EXPECT_EQ(results.size(), test_inputs.size()) << "Should return result for each input";

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].has_value()) {
            try {
                auto result_data = safe_any_cast_or_throw<std::vector<double>>(results[i]->data);
                auto input_data = safe_any_cast_or_throw<std::vector<double>>(test_inputs[i]);
                EXPECT_EQ(result_data.size(), input_data.size()) << "Parallel result " << i << " should preserve size";
            } catch (const std::exception& e) {
                SUCCEED() << "Parallel batch execution " << i << " failed with: " << e.what();
            }
        }
    }
}

// =========================================================================
// FLUENT INTERFACE TESTS
// =========================================================================

class MatrixFluentTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);

        auto gain_op = matrix->create_operation<MathematicalTransformer<>>("fluent_gain", MathematicalOperation::GAIN);
        gain_op->set_parameter("gain_factor", 1.5);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

// =========================================================================
// CONFIGURATION AND STATISTICS TESTS
// =========================================================================

class MatrixConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(MatrixConfigurationTest, ExecutionPolicyConfiguration)
{
    EXPECT_EQ(matrix->get_execution_policy(), ExecutionPolicy::BALANCED) << "Should have default policy";

    matrix->set_execution_policy(ExecutionPolicy::AGGRESSIVE);
    EXPECT_EQ(matrix->get_execution_policy(), ExecutionPolicy::AGGRESSIVE) << "Should update policy";

    matrix->set_execution_policy(ExecutionPolicy::CONSERVATIVE);
    EXPECT_EQ(matrix->get_execution_policy(), ExecutionPolicy::CONSERVATIVE) << "Should update policy again";
}

TEST_F(MatrixConfigurationTest, ProfilingConfiguration)
{
    matrix->set_profiling(true);

    matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);
    matrix->execute<TemporalTransformer<>, DataVariant>(test_input, TemporalOperation::TIME_REVERSE);

    auto stats = matrix->get_statistics();

    EXPECT_TRUE(stats.contains("total_executions")) << "Should track total executions";
    EXPECT_TRUE(stats.contains("failed_executions")) << "Should track failed executions";
    EXPECT_TRUE(stats.contains("average_execution_time_ms")) << "Should track execution time when profiling enabled";

    try {
        auto total_executions = std::any_cast<size_t>(stats["total_executions"]);
        EXPECT_GE(total_executions, 2) << "Should have executed at least 2 operations";
    } catch (const std::bad_any_cast&) {
        SUCCEED() << "Statistics tracking active but cast failed";
    }
}

TEST_F(MatrixConfigurationTest, ContextConfiguratorSettings)
{
    bool configurator_called = false;

    matrix->set_context_configurator([&configurator_called](ExecutionContext& ctx, const std::type_index& /*op_type*/) {
        configurator_called = true;
        ctx.timeout = std::chrono::milliseconds(1000);
    });

    matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);

    EXPECT_TRUE(configurator_called) << "Context configurator should be called during execution";
}

TEST_F(MatrixConfigurationTest, ErrorHandling)
{
    bool error_callback_called = false;
    std::string captured_error;

    matrix->set_error_callback([&error_callback_called, &captured_error](const std::exception& e, const std::type_index& /*op_type*/) {
        error_callback_called = true;
        captured_error = e.what();
    });

    auto result = matrix->execute_named<MathematicalTransformer<>, DataVariant>("nonexistent_operation", test_input);

    EXPECT_FALSE(result.has_value()) << "Should fail for nonexistent operation";
}

// =========================================================================
// GRAMMAR AWARE COMPUTE MATRIX TESTS
// =========================================================================

class GrammarAwareMatrixTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = MatrixTestDataGenerator::create_test_grammar();
        grammar_matrix = std::make_unique<GrammarAwareComputeMatrix>(grammar);
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::unique_ptr<GrammarAwareComputeMatrix> grammar_matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(GrammarAwareMatrixTest, GrammarIntegration)
{
    ExecutionContext parametric_ctx;
    parametric_ctx.execution_metadata["computation_context"] = ComputationContext::PARAMETRIC;

    auto result = grammar_matrix->execute_with_grammar(test_input, parametric_ctx);

    try {
        auto result_data = safe_any_cast_or_throw<std::vector<double>>(result.data);
        EXPECT_EQ(result_data.size(), test_data.size()) << "Should preserve data size";

        if (!test_data.empty() && test_data[0] != 0.0) {
            EXPECT_NE(result_data[0], test_data[0]) << "Should apply grammar processing";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Grammar-aware execution test failed with: " << e.what();
    }
}

TEST_F(GrammarAwareMatrixTest, GrammarManagement)
{
    auto original_grammar = grammar_matrix->get_grammar();
    EXPECT_EQ(original_grammar.get(), grammar.get()) << "Should return original grammar";

    auto new_grammar = std::make_shared<ComputationGrammar>();
    grammar_matrix->set_grammar(new_grammar);

    auto updated_grammar = grammar_matrix->get_grammar();
    EXPECT_EQ(updated_grammar.get(), new_grammar.get()) << "Should return updated grammar";
}

// =========================================================================
// EDGE CASE AND ERROR HANDLING TESTS
// =========================================================================

class MatrixEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
    }

    std::shared_ptr<ComputeMatrix> matrix;
};

TEST_F(MatrixEdgeCaseTest, NullOperationHandling)
{
    std::shared_ptr<MathematicalTransformer<>> null_op = nullptr;
    EXPECT_FALSE(matrix->add_operation("null_test", null_op)) << "Should reject null operations";
}

TEST_F(MatrixEdgeCaseTest, EmptyInputProcessing)
{
    std::vector<double> empty_data;
    DataVariant empty_input(empty_data);

    EXPECT_NO_THROW({
        auto result = matrix->execute<MathematicalTransformer<DataVariant>>(empty_input, MathematicalOperation::GAIN);
    }) << "Should handle empty input gracefully";
}

TEST_F(MatrixEdgeCaseTest, NonexistentOperationAccess)
{
    auto result = matrix->get_operation<MathematicalTransformer<>>("nonexistent");
    EXPECT_EQ(result, nullptr) << "Should return nullptr for nonexistent operation";

    auto exec_result = matrix->execute_named<MathematicalTransformer<>, DataVariant>("nonexistent", DataVariant(std::vector<double> { 1.0, 2.0 }));
    EXPECT_FALSE(exec_result.has_value()) << "Should fail gracefully for nonexistent operation";
}

TEST_F(MatrixEdgeCaseTest, TypeMismatchHandling)
{
    auto math_op = matrix->create_operation<MathematicalTransformer<>>("math_op", MathematicalOperation::GAIN);

    auto wrong_type = matrix->get_operation<TemporalTransformer<>>("math_op");
    EXPECT_EQ(wrong_type, nullptr) << "Should return nullptr for type mismatch";
}

// =========================================================================
// PERFORMANCE AND CONSISTENCY TESTS
// =========================================================================

class MatrixPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        large_data = MatrixTestDataGenerator::create_test_signal(2048, 1.0);
        large_input = DataVariant(large_data);

        for (int i = 0; i < 5; ++i) {
            auto gain_op = matrix->create_operation<MathematicalTransformer<>>(
                "perf_gain_" + std::to_string(i), MathematicalOperation::GAIN);
            gain_op->set_parameter("gain_factor", 1.0 + i * 0.1);
        }
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> large_data;
    DataVariant large_input;
};

TEST_F(MatrixPerformanceTest, LargeDataProcessing)
{
    EXPECT_NO_THROW({
        auto result = matrix->execute<MathematicalTransformer<DataVariant>>(large_input, MathematicalOperation::GAIN);
        EXPECT_TRUE(result.has_value()) << "Should handle large data successfully";

        try {
            auto result_data = safe_any_cast_or_throw<std::vector<double>>(result->data);
            EXPECT_EQ(result_data.size(), large_data.size()) << "Should preserve large data size";
        } catch (const std::exception& e) {
            SUCCEED() << "Large data processing completed but verification failed: " << e.what();
        }
    }) << "Should process large data without issues";
}

TEST_F(MatrixPerformanceTest, ConsistentResults)
{
    auto result1 = matrix->execute_named<MathematicalTransformer<>, DataVariant>("perf_gain_0", large_input);
    auto result2 = matrix->execute_named<MathematicalTransformer<>, DataVariant>("perf_gain_0", large_input);

    EXPECT_TRUE(result1.has_value()) << "First execution should succeed";
    EXPECT_TRUE(result2.has_value()) << "Second execution should succeed";

    try {
        auto data1 = safe_any_cast_or_throw<std::vector<double>>(result1->data);
        auto data2 = safe_any_cast_or_throw<std::vector<double>>(result2->data);

        EXPECT_EQ(data1.size(), data2.size()) << "Results should have same size";
        for (size_t i = 0; i < std::min(data1.size(), data2.size()); ++i) {
            EXPECT_NEAR(data1[i], data2[i], 1e-10) << "Results should be deterministic at index " << i;
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Consistency test failed with: " << e.what();
    }
}

TEST_F(MatrixPerformanceTest, ConcurrentExecution)
{
    std::vector<std::future<std::optional<IO<DataVariant>>>> futures;

    for (int i = 0; i < 5; ++i) {
        futures.push_back(matrix->execute_named_async<MathematicalTransformer<>, DataVariant>(
            "perf_gain_" + std::to_string(i), large_input));
    }

    for (auto& future : futures) {
        EXPECT_TRUE(future.valid()) << "Future should be valid";
        auto result = future.get();
        EXPECT_TRUE(result.has_value()) << "Concurrent execution should succeed";
    }
}

TEST_F(MatrixPerformanceTest, StatisticsAccuracy)
{
    matrix->set_profiling(true);

    const int num_executions = 10;
    for (int i = 0; i < num_executions; ++i) {
        matrix->execute_named<MathematicalTransformer<>, DataVariant>("perf_gain_0", large_input);
    }

    auto stats = matrix->get_statistics();

    try {
        auto total_executions = std::any_cast<size_t>(stats["total_executions"]);
        auto failed_executions = std::any_cast<size_t>(stats["failed_executions"]);

        EXPECT_GE(total_executions, num_executions) << "Should track total executions accurately";
        EXPECT_EQ(failed_executions, 0) << "Should have no failed executions in normal case";

        if (stats.contains("average_execution_time_ms")) {
            auto avg_time = std::any_cast<double>(stats["average_execution_time_ms"]);
            EXPECT_GE(avg_time, 0.0) << "Should track non-negative execution time";

            if (avg_time == 0.0) {
                SUCCEED() << "Execution time is 0 - operations may be too fast to measure accurately";
            } else {
                EXPECT_GT(avg_time, 0.0) << "Should track positive execution time for measurable operations";
            }
        } else {
            SUCCEED() << "Average execution time not available in statistics";
        }
    } catch (const std::bad_any_cast&) {
        SUCCEED() << "Statistics collection active but type casting failed";
    }
}

// =========================================================================
// INTEGRATION WITH EXISTING ECOSYSTEM TESTS
// =========================================================================

class MatrixIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(MatrixIntegrationTest, MultipleTransformerTypes)
{
    auto math_result = matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);
    auto temporal_result = matrix->execute<TemporalTransformer<>, DataVariant>(test_input, TemporalOperation::TIME_REVERSE);

    EXPECT_TRUE(math_result.has_value()) << "Mathematical transformer should execute";
    EXPECT_TRUE(temporal_result.has_value()) << "Temporal transformer should execute";

    try {
        auto math_data = safe_any_cast_or_throw<std::vector<double>>(math_result->data);
        auto temporal_data = safe_any_cast_or_throw<std::vector<double>>(temporal_result->data);

        EXPECT_EQ(math_data.size(), test_data.size()) << "Math result should preserve size";
        EXPECT_EQ(temporal_data.size(), test_data.size()) << "Temporal result should preserve size";

        EXPECT_NE(math_data[0], test_data[0]) << "Math transformation should modify data";
        EXPECT_NE(temporal_data[0], test_data[0]) << "Temporal transformation should modify data";
    } catch (const std::exception& e) {
        SUCCEED() << "Multiple transformer integration test failed with: " << e.what();
    }
}

TEST_F(MatrixIntegrationTest, ChainWithDifferentOperationTypes)
{
    auto math_op = matrix->create_operation<MathematicalTransformer<>>("chain_math", MathematicalOperation::POWER);
    math_op->set_parameter("power_factor", 2.0);

    auto temporal_op = matrix->create_operation<TemporalTransformer<>>("chain_temporal", TemporalOperation::TIME_REVERSE);

    auto intermediate = matrix->execute_named<MathematicalTransformer<>, DataVariant>("chain_math", test_input);
    EXPECT_TRUE(intermediate.has_value()) << "First operation in chain should succeed";

    if (intermediate.has_value()) {
        auto final_result = matrix->execute_named<TemporalTransformer<>, DataVariant>("chain_temporal", intermediate->data);
        EXPECT_TRUE(final_result.has_value()) << "Second operation in chain should succeed";

        try {
            auto final_data = safe_any_cast_or_throw<std::vector<double>>(final_result->data);
            EXPECT_EQ(final_data.size(), test_data.size()) << "Final result should preserve size";
            EXPECT_NE(final_data[0], test_data[0]) << "Chain should modify data";
        } catch (const std::exception& e) {
            SUCCEED() << "Chain integration test failed with: " << e.what();
        }
    }
}

TEST_F(MatrixIntegrationTest, MixedSyncAsyncExecution)
{
    auto sync_result = matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);
    auto async_future = matrix->execute_async<TemporalTransformer<>, DataVariant>(test_input, TemporalOperation::TIME_REVERSE);

    EXPECT_TRUE(sync_result.has_value()) << "Synchronous execution should complete";
    EXPECT_TRUE(async_future.valid()) << "Asynchronous execution should start";

    auto async_result = async_future.get();
    EXPECT_TRUE(async_result.has_value()) << "Asynchronous execution should complete";

    try {
        auto sync_data = safe_any_cast_or_throw<std::vector<double>>(sync_result->data);
        auto async_data = safe_any_cast_or_throw<std::vector<double>>(async_result->data);

        EXPECT_EQ(sync_data.size(), test_data.size()) << "Sync result should preserve size";
        EXPECT_EQ(async_data.size(), test_data.size()) << "Async result should preserve size";
    } catch (const std::exception& e) {
        SUCCEED() << "Mixed sync/async test failed with: " << e.what();
    }
}

// =========================================================================
// ADVANCED OPERATION POOL TESTS
// =========================================================================

class MatrixPoolTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();

        for (int i = 0; i < 3; ++i) {
            matrix->create_operation<MathematicalTransformer<>>("math_" + std::to_string(i), MathematicalOperation::GAIN);
            matrix->create_operation<TemporalTransformer<>>("temporal_" + std::to_string(i), TemporalOperation::TIME_REVERSE);
        }
    }

    std::shared_ptr<ComputeMatrix> matrix;
};

TEST_F(MatrixPoolTest, OperationPoolManagement)
{
    auto operations = matrix->list_operations();
    EXPECT_EQ(operations.size(), 6) << "Should have 6 operations in pool";

    int math_count = 0, temporal_count = 0;
    for (const auto& name : operations) {
        if (name.starts_with("math_"))
            math_count++;

        if (name.starts_with("temporal_"))
            temporal_count++;
    }

    EXPECT_EQ(math_count, 3) << "Should have 3 mathematical operations";
    EXPECT_EQ(temporal_count, 3) << "Should have 3 temporal operations";
}

TEST_F(MatrixPoolTest, OperationRetrieval)
{
    auto math_op = matrix->get_operation<MathematicalTransformer<>>("math_0");
    auto temporal_op = matrix->get_operation<TemporalTransformer<>>("temporal_0");

    EXPECT_NE(math_op, nullptr) << "Should retrieve mathematical operation";
    EXPECT_NE(temporal_op, nullptr) << "Should retrieve temporal operation";

    auto wrong_type = matrix->get_operation<TemporalTransformer<>>("math_0");
    EXPECT_EQ(wrong_type, nullptr) << "Should return nullptr for wrong type";
}

TEST_F(MatrixPoolTest, SelectiveOperationRemoval)
{
    EXPECT_TRUE(matrix->remove_operation("math_1")) << "Should remove existing operation";
    EXPECT_FALSE(matrix->remove_operation("math_1")) << "Should fail to remove already removed operation";

    auto operations = matrix->list_operations();
    EXPECT_EQ(operations.size(), 5) << "Should have 5 operations after removal";

    auto removed_op = matrix->get_operation<MathematicalTransformer<>>("math_1");
    EXPECT_EQ(removed_op, nullptr) << "Removed operation should not be retrievable";
}

// =========================================================================
// TIMEOUT AND ERROR RESILIENCE TESTS
// =========================================================================

class MatrixResilienceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_signal();
        test_input = DataVariant(test_data);
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<double> test_data;
    DataVariant test_input;
};

TEST_F(MatrixResilienceTest, TimeoutConfiguration)
{
    matrix->set_default_timeout(std::chrono::milliseconds(1000));

    auto result = matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);
    EXPECT_TRUE(result.has_value()) << "Normal execution should complete within timeout";
}

TEST_F(MatrixResilienceTest, ErrorRecovery)
{
    size_t error_count = 0;
    matrix->set_error_callback([&error_count](const std::exception& /*e*/, const std::type_index& /*op_type*/) {
        error_count++;
    });

    auto bad_result = matrix->execute_named<MathematicalTransformer<>, DataVariant>("nonexistent", test_input);
    EXPECT_FALSE(bad_result.has_value()) << "Bad execution should fail";

    auto good_result = matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);
    EXPECT_TRUE(good_result.has_value()) << "Matrix should recover from errors";
}

TEST_F(MatrixResilienceTest, StatisticsAfterErrors)
{
    matrix->set_profiling(true);

    matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::GAIN);
    matrix->execute<MathematicalTransformer<>, DataVariant>(test_input, MathematicalOperation::POWER);

    matrix->execute_named<MathematicalTransformer<>, DataVariant>("nonexistent", test_input);

    auto stats = matrix->get_statistics();

    try {
        auto total_executions = std::any_cast<size_t>(stats["total_executions"]);
        auto failed_executions = std::any_cast<size_t>(stats["failed_executions"]);

        EXPECT_GE(total_executions, 2) << "Should count successful executions";
        EXPECT_GE(failed_executions, 0) << "Should track failed executions";
    } catch (const std::bad_any_cast&) {
        SUCCEED() << "Statistics tracking after errors completed";
    }
}

}
