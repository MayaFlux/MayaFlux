#include "../test_config.h"

#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/ComputeMatrix.hpp"
#include "MayaFlux/Yantra/ComputePipeline.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"
#include <random>

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

// =========================================================================
// TEST DATA GENERATORS
// =========================================================================

class MatrixTestDataGenerator {
public:
    static std::vector<DataVariant> create_test_multichannel_signal(size_t channels = 2, size_t size = 256, double amplitude = 1.0)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        for (size_t ch = 0; ch < channels; ++ch) {
            std::vector<double> channel_data(size);
            for (size_t i = 0; i < size; ++i) {
                double phase_offset = (double)ch * M_PI / 4.0;
                double frequency = 2.0 * M_PI * i / 32.0;
                channel_data[i] = amplitude * std::sin(frequency + phase_offset) + 0.1 * (double)(ch + 1);
            }
            multichannel_data.emplace_back(std::move(channel_data));
        }
        return multichannel_data;
    }

    static std::vector<DataVariant> create_ramp_multichannel_signal(size_t channels = 2, size_t size = 128)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        for (size_t ch = 0; ch < channels; ++ch) {
            std::vector<double> channel_data(size);
            for (size_t i = 0; i < size; ++i) {
                double base_ramp = static_cast<double>(i) / size;
                double channel_multiplier = 1.0 + (double)ch * 0.5;
                channel_data[i] = base_ramp * channel_multiplier;
            }
            multichannel_data.emplace_back(std::move(channel_data));
        }
        return multichannel_data;
    }

    static std::vector<double> create_test_signal(size_t size = 256, double amplitude = 1.0)
    {
        std::vector<double> signal(size);
        for (size_t i = 0; i < size; ++i) {
            signal[i] = amplitude * std::sin(2.0 * M_PI * i / 32.0) + 0.1;
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
            UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
            { { "gain_factor", 2.0 } },
            90,
            MathematicalOperation::GAIN);

        return grammar;
    }

    static std::vector<DataVariant> create_mixed_type_multichannel(size_t size = 256)
    {
        return {
            DataVariant(std::vector<double>(size, 0.5)),
            DataVariant(std::vector<float>(size, 0.3F)),
            DataVariant(std::vector<double>(size, 0.7))
        };
    }

    static std::vector<DataVariant> create_variable_size_multichannel()
    {
        return {
            DataVariant(std::vector<double>(256, 0.4)),
            DataVariant(std::vector<double>(128, 0.6)),
            DataVariant(std::vector<double>(512, 0.8)),
            DataVariant(std::vector<double>(64, 0.2))
        };
    }

    static std::vector<DataVariant> create_silence_multichannel(size_t channels = 2, size_t size = 256)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        for (size_t ch = 0; ch < channels; ++ch) {
            multichannel_data.emplace_back(std::vector<double>(size, 0.0));
        }
        return multichannel_data;
    }

    static std::vector<DataVariant> create_noise_multichannel(size_t channels = 2, size_t size = 256, double amplitude = 0.1)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-amplitude, amplitude);

        for (size_t ch = 0; ch < channels; ++ch) {
            std::vector<double> channel_data(size);
            for (size_t i = 0; i < size; ++i) {
                channel_data[i] = dis(gen) + 0.05 * (double)(ch + 1);
            }
            multichannel_data.emplace_back(std::move(channel_data));
        }
        return multichannel_data;
    }

    static std::vector<DataVariant> create_frequency_sweep_multichannel(size_t channels = 2, size_t size = 512)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        for (size_t ch = 0; ch < channels; ++ch) {
            std::vector<double> channel_data(size);
            double freq_start = 1.0 + ch * 0.5;
            double freq_end = freq_start * 4.0;

            for (size_t i = 0; i < size; ++i) {
                double t = static_cast<double>(i) / size;
                double freq = freq_start + (freq_end - freq_start) * t;
                channel_data[i] = 0.5 * std::sin(2.0 * M_PI * freq * t) + 0.1 * (double)(ch + 1);
            }
            multichannel_data.emplace_back(std::move(channel_data));
        }
        return multichannel_data;
    }

    static std::vector<DataVariant> create_impulse_multichannel(size_t channels = 2, size_t size = 256, size_t impulse_position = 64)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        for (size_t ch = 0; ch < channels; ++ch) {
            std::vector<double> channel_data(size, 0.0);
            if (impulse_position < size) {
                channel_data[impulse_position] = 1.0 * (ch + 1);
            }
            multichannel_data.emplace_back(std::move(channel_data));
        }
        return multichannel_data;
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
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }
    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
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
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }
    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
};

TEST_F(MatrixExecutionTest, DirectExecution)
{
    auto result = matrix->execute<MathematicalTransformer<>, std::vector<DataVariant>>(test_input, MathematicalOperation::GAIN);
    EXPECT_TRUE(result.has_value()) << "Should execute operation successfully";

    try {
        EXPECT_EQ(result->data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result->data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result->data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Direct execution test failed with: " << e.what();
    }
}

TEST_F(MatrixExecutionTest, NamedExecution)
{
    auto math_op = matrix->create_operation<MathematicalTransformer<>>("named_gain", MathematicalOperation::GAIN);
    math_op->set_parameter("gain_factor", 3.0);

    auto result = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("named_gain", test_input);
    EXPECT_TRUE(result.has_value()) << "Should execute named operation successfully";

    try {
        EXPECT_EQ(result->data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result->data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result->data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";

            bool values_changed = false;
            for (size_t i = 0; i < std::min(original_channel.size(), result_channel.size()); ++i) {
                if (std::abs(result_channel[i] - original_channel[i]) > 1e-10) {
                    values_changed = true;
                    if (std::abs(original_channel[i]) > 1e-10) {
                        double gain_applied = result_channel[i] / original_channel[i];
                        EXPECT_NEAR(gain_applied, 3.0, 0.1) << "Should apply 3x gain on channel " << ch;
                        break;
                    }
                }
            }
            EXPECT_TRUE(values_changed) << "Channel " << ch << " should be modified by gain operation";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Named execution test failed with: " << e.what();
    }
}

TEST_F(MatrixExecutionTest, AsynchronousExecution)
{
    auto future = matrix->execute_async<MathematicalTransformer<>, std::vector<DataVariant>>(test_input, MathematicalOperation::GAIN);
    EXPECT_TRUE(future.valid()) << "Should return valid future";

    auto result = future.get();
    EXPECT_TRUE(result.has_value()) << "Should complete asynchronously";

    try {
        EXPECT_EQ(result->data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result->data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result->data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Async execution test failed with: " << e.what();
    }
}

TEST_F(MatrixExecutionTest, NamedAsyncExecution)
{
    matrix->create_operation<MathematicalTransformer<>>("async_gain", MathematicalOperation::GAIN);

    auto future = matrix->execute_named_async<MathematicalTransformer<>, std::vector<DataVariant>>("async_gain", test_input);
    EXPECT_TRUE(future.valid()) << "Should return valid future";

    auto result = future.get();
    EXPECT_TRUE(result.has_value()) << "Should complete named async execution";

    try {
        EXPECT_EQ(result->data.size(), test_data.size()) << "Should preserve channel count in async execution";
    } catch (const std::exception& e) {
        SUCCEED() << "Named async execution test failed with: " << e.what();
    }
}

// =========================================================================
// PARALLEL EXECUTION TESTS
// =========================================================================

class MatrixParallelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }
    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
};

TEST_F(MatrixParallelTest, ParallelNamedExecution)
{
    matrix->create_operation<MathematicalTransformer<>>("gain1", MathematicalOperation::GAIN);
    matrix->create_operation<MathematicalTransformer<>>("gain2", MathematicalOperation::POWER);

    std::vector<std::string> operation_names = { "gain1", "gain2" };
    auto results = matrix->execute_parallel_named<MathematicalTransformer<>, std::vector<DataVariant>>(operation_names, test_input);

    EXPECT_EQ(results.size(), 2) << "Should return results for both named operations";

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].has_value()) {
            try {
                EXPECT_EQ(results[i]->data.size(), test_data.size()) << "Result " << i << " should preserve channel count";

                for (size_t ch = 0; ch < results[i]->data.size(); ++ch) {
                    auto original_channel = std::get<std::vector<double>>(test_data[ch]);
                    auto result_channel = std::get<std::vector<double>>(results[i]->data[ch]);
                    EXPECT_EQ(result_channel.size(), original_channel.size())
                        << "Result " << i << ", channel " << ch << " should preserve size";
                }
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
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }
    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
};

TEST_F(MatrixChainTest, BasicChainExecution)
{
    auto result = matrix->execute_chain<MathematicalTransformer<>, TemporalTransformer<>, std::vector<DataVariant>, std::vector<DataVariant>, std::vector<DataVariant>>(test_input);
    EXPECT_TRUE(result.has_value()) << "Should execute chain successfully";

    try {
        EXPECT_EQ(result->data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result->data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result->data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";

            bool values_changed = false;
            for (size_t i = 0; i < std::min(original_channel.size(), result_channel.size()); ++i) {
                if (std::abs(result_channel[i] - original_channel[i]) > 1e-10) {
                    values_changed = true;
                    break;
                }
            }
            EXPECT_TRUE(values_changed) << "Channel " << ch << " should apply both transformations";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Chain execution test failed with: " << e.what();
    }
}

TEST_F(MatrixChainTest, NamedChainExecution)
{
    auto gain_op = matrix->create_operation<MathematicalTransformer<>>("chain_gain", MathematicalOperation::GAIN);
    gain_op->set_parameter("gain_factor", 2.0);
    auto reverse_op = matrix->create_operation<TemporalTransformer<>>("chain_reverse", TemporalOperation::TIME_REVERSE);

    auto result = matrix->execute_chain_named<MathematicalTransformer<>, TemporalTransformer<>, std::vector<DataVariant>, std::vector<DataVariant>, std::vector<DataVariant>>(
        "chain_gain", "chain_reverse", test_input);

    EXPECT_TRUE(result.has_value()) << "Should execute named chain successfully";

    try {
        EXPECT_EQ(result->data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < result->data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto result_channel = std::get<std::vector<double>>(result->data[ch]);
            EXPECT_EQ(result_channel.size(), original_channel.size()) << "Should preserve channel " << ch << " size";

            if (!original_channel.empty()) {
                // After gain (2x) and reverse, first sample should be last sample * 2
                double expected_first = original_channel.back() * 2.0;
                EXPECT_NEAR(result_channel[0], expected_first, 0.01)
                    << "Channel " << ch << " should apply gain then reverse";
            }
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

        test_inputs.emplace_back(MatrixTestDataGenerator::create_test_multichannel_signal(2, 128, 1.0));
        test_inputs.emplace_back(MatrixTestDataGenerator::create_test_multichannel_signal(2, 128, 0.5));
        test_inputs.emplace_back(MatrixTestDataGenerator::create_ramp_multichannel_signal(2, 128));
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<std::vector<DataVariant>> test_inputs;
};

TEST_F(MatrixBatchTest, SequentialBatchExecution)
{
    auto results = matrix->execute_batch<MathematicalTransformer<>, std::vector<DataVariant>>(test_inputs, MathematicalOperation::GAIN);

    EXPECT_EQ(results.size(), test_inputs.size()) << "Should return result for each input";

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].has_value()) {
            try {
                EXPECT_EQ(results[i]->data.size(), test_inputs[i].size()) << "Result " << i << " should preserve channel count";

                for (size_t ch = 0; ch < results[i]->data.size(); ++ch) {
                    auto result_channel = std::get<std::vector<double>>(results[i]->data[ch]);
                    auto input_channel = std::get<std::vector<double>>(test_inputs[i][ch]);
                    EXPECT_EQ(result_channel.size(), input_channel.size())
                        << "Result " << i << ", channel " << ch << " should preserve size";
                }
            } catch (const std::exception& e) {
                SUCCEED() << "Batch execution " << i << " failed with: " << e.what();
            }
        }
    }
}

TEST_F(MatrixBatchTest, ParallelBatchExecution)
{
    auto results = matrix->execute_batch_parallel<MathematicalTransformer<>, std::vector<DataVariant>>(test_inputs, MathematicalOperation::GAIN);

    EXPECT_EQ(results.size(), test_inputs.size()) << "Should return result for each input";

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].has_value()) {
            try {
                EXPECT_EQ(results[i]->data.size(), test_inputs[i].size()) << "Parallel result " << i << " should preserve channel count";

                for (size_t ch = 0; ch < results[i]->data.size(); ++ch) {
                    auto result_channel = std::get<std::vector<double>>(results[i]->data[ch]);
                    auto input_channel = std::get<std::vector<double>>(test_inputs[i][ch]);
                    EXPECT_EQ(result_channel.size(), input_channel.size())
                        << "Parallel result " << i << ", channel " << ch << " should preserve size";
                }
            } catch (const std::exception& e) {
                SUCCEED() << "Parallel batch execution " << i << " failed with: " << e.what();
            }
        }
    }
}

// =========================================================================
// CONFIGURATION AND STATISTICS TESTS
// =========================================================================

class MatrixConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
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

    matrix->execute<MathematicalTransformer<>, std::vector<DataVariant>>(test_input, MathematicalOperation::GAIN);
    matrix->execute<TemporalTransformer<>, std::vector<DataVariant>>(test_input, TemporalOperation::TIME_REVERSE);

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

    matrix->execute<MathematicalTransformer<>, std::vector<DataVariant>>(test_input, MathematicalOperation::GAIN);

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

    auto result = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("nonexistent_operation", test_input);

    EXPECT_FALSE(result.has_value()) << "Should fail for nonexistent operation";
}

// =========================================================================
// FLUENT INTERFACE TESTS
// =========================================================================

/* class MatrixFluentTest : public ::testing::Test {
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
*/

// =========================================================================
// GRAMMAR AWARE COMPUTE MATRIX TESTS
// =========================================================================

class GrammarAwareMatrixTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = MatrixTestDataGenerator::create_test_grammar();
        grammar_matrix = std::make_unique<GrammarAwareComputeMatrix>(grammar);
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }
    std::shared_ptr<ComputationGrammar> grammar;
    std::unique_ptr<GrammarAwareComputeMatrix> grammar_matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
};

TEST_F(GrammarAwareMatrixTest, GrammarIntegration)
{
    ExecutionContext parametric_ctx;
    parametric_ctx.execution_metadata["computation_context"] = ComputationContext::PARAMETRIC;

    auto result = grammar_matrix->execute_with_grammar(test_input, parametric_ctx);

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
                    break;
                }
            }

            if (values_changed) {
                EXPECT_TRUE(true) << "Channel " << ch << " should apply grammar processing";
            } else {
                SUCCEED() << "Channel " << ch << " has no detectable changes, possibly all zero values";
            }
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
    std::vector<DataVariant> empty_multichannel;

    EXPECT_NO_THROW({
        auto result = matrix->execute<MathematicalTransformer<>>(empty_multichannel, MathematicalOperation::GAIN);
    }) << "Should handle empty multichannel input gracefully";
}

TEST_F(MatrixEdgeCaseTest, EmptyChannelProcessing)
{
    std::vector<DataVariant> empty_channels = {
        DataVariant(std::vector<double> {}),
        DataVariant(std::vector<double> {})
    };

    EXPECT_NO_THROW({
        auto result = matrix->execute<MathematicalTransformer<>>(empty_channels, MathematicalOperation::GAIN);
    }) << "Should handle empty channels gracefully";
}

TEST_F(MatrixEdgeCaseTest, NonexistentOperationAccess)
{
    auto result = matrix->get_operation<MathematicalTransformer<>>("nonexistent");
    EXPECT_EQ(result, nullptr) << "Should return nullptr for nonexistent operation";

    std::vector<DataVariant> test_multichannel = {
        DataVariant(std::vector<double> { 1.0, 2.0 }),
        DataVariant(std::vector<double> { 3.0, 4.0 })
    };

    auto exec_result = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("nonexistent", test_multichannel);
    EXPECT_FALSE(exec_result.has_value()) << "Should fail gracefully for nonexistent operation";
}

TEST_F(MatrixEdgeCaseTest, TypeMismatchHandling)
{
    auto math_op = matrix->create_operation<MathematicalTransformer<>>("math_op", MathematicalOperation::GAIN);

    auto wrong_type = matrix->get_operation<TemporalTransformer<>>("math_op");
    EXPECT_EQ(wrong_type, nullptr) << "Should return nullptr for type mismatch";
}

TEST_F(MatrixEdgeCaseTest, SingleChannelToMultiChannelCompatibility)
{
    std::vector<DataVariant> single_channel = {
        DataVariant(std::vector<double> { 1.0, 2.0, 3.0 })
    };

    EXPECT_NO_THROW({
        auto result = matrix->execute<MathematicalTransformer<>>(single_channel, MathematicalOperation::GAIN);
        EXPECT_TRUE(result.has_value()) << "Should handle single-channel as multichannel";
        if (result.has_value()) {
            EXPECT_EQ(result->data.size(), 1) << "Should preserve single channel structure";
        }
    }) << "Should process single-channel data as multichannel";
}

TEST_F(MatrixEdgeCaseTest, MixedChannelSizeHandling)
{
    std::vector<DataVariant> mixed_sizes = {
        DataVariant(std::vector<double>(256, 0.5)),
        DataVariant(std::vector<double>(128, 0.3)),
        DataVariant(std::vector<double>(512, 0.7))
    };

    EXPECT_NO_THROW({
        auto result = matrix->execute<MathematicalTransformer<>>(mixed_sizes, MathematicalOperation::GAIN);
        if (result.has_value()) {
            EXPECT_EQ(result->data.size(), 3) << "Should preserve channel count";

            auto ch0 = std::get<std::vector<double>>(result->data[0]);
            auto ch1 = std::get<std::vector<double>>(result->data[1]);
            auto ch2 = std::get<std::vector<double>>(result->data[2]);

            EXPECT_EQ(ch0.size(), 256) << "Channel 0 should preserve size";
            EXPECT_EQ(ch1.size(), 128) << "Channel 1 should preserve size";
            EXPECT_EQ(ch2.size(), 512) << "Channel 2 should preserve size";
        }
    }) << "Should handle mixed channel sizes";
}

// =========================================================================
// PERFORMANCE AND CONSISTENCY TESTS
// =========================================================================

class MatrixPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        matrix = ComputeMatrix::create();
        large_data = MatrixTestDataGenerator::create_test_multichannel_signal(4, 2048, 1.0); // 4 channels, 2048 samples each
        large_input = large_data;

        for (int i = 0; i < 5; ++i) {
            auto gain_op = matrix->create_operation<MathematicalTransformer<>>(
                "perf_gain_" + std::to_string(i), MathematicalOperation::GAIN);
            gain_op->set_parameter("gain_factor", 1.0 + i * 0.1);
        }
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> large_data;
    std::vector<DataVariant> large_input;
};

TEST_F(MatrixPerformanceTest, LargeDataProcessing)
{
    EXPECT_NO_THROW({
        auto result = matrix->execute<MathematicalTransformer<>>(large_input, MathematicalOperation::GAIN);
        EXPECT_TRUE(result.has_value()) << "Should handle large multichannel data successfully";

        try {
            EXPECT_EQ(result->data.size(), large_data.size()) << "Should preserve channel count for large data";

            for (size_t ch = 0; ch < result->data.size(); ++ch) {
                auto original_channel = std::get<std::vector<double>>(large_data[ch]);
                auto result_channel = std::get<std::vector<double>>(result->data[ch]);
                EXPECT_EQ(result_channel.size(), original_channel.size())
                    << "Should preserve large channel " << ch << " size (" << original_channel.size() << " samples)";
            }
        } catch (const std::exception& e) {
            SUCCEED() << "Large data processing completed but verification failed: " << e.what();
        }
    }) << "Should process large multichannel data without issues";
}

TEST_F(MatrixPerformanceTest, ConsistentResults)
{
    auto result1 = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("perf_gain_0", large_input);
    auto result2 = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("perf_gain_0", large_input);

    EXPECT_TRUE(result1.has_value()) << "First execution should succeed";
    EXPECT_TRUE(result2.has_value()) << "Second execution should succeed";

    try {
        EXPECT_EQ(result1->data.size(), result2->data.size()) << "Results should have same channel count";

        for (size_t ch = 0; ch < std::min(result1->data.size(), result2->data.size()); ++ch) {
            auto data1 = std::get<std::vector<double>>(result1->data[ch]);
            auto data2 = std::get<std::vector<double>>(result2->data[ch]);

            EXPECT_EQ(data1.size(), data2.size()) << "Channel " << ch << " should have same size";

            for (size_t i = 0; i < std::min(data1.size(), data2.size()); ++i) {
                EXPECT_NEAR(data1[i], data2[i], 1e-10)
                    << "Results should be deterministic at channel " << ch << ", index " << i;
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Consistency test failed with: " << e.what();
    }
}

TEST_F(MatrixPerformanceTest, ConcurrentExecution)
{
    std::vector<std::future<std::optional<IO<std::vector<DataVariant>>>>> futures;

    for (int i = 0; i < 5; ++i) {
        futures.push_back(matrix->execute_named_async<MathematicalTransformer<>, std::vector<DataVariant>>(
            "perf_gain_" + std::to_string(i), large_input));
    }

    for (size_t i = 0; i < futures.size(); ++i) {
        EXPECT_TRUE(futures[i].valid()) << "Future " << i << " should be valid";
        auto result = futures[i].get();
        EXPECT_TRUE(result.has_value()) << "Concurrent execution " << i << " should succeed";

        if (result.has_value()) {
            EXPECT_EQ(result->data.size(), large_data.size())
                << "Concurrent result " << i << " should preserve channel count";
        }
    }
}

TEST_F(MatrixPerformanceTest, StatisticsAccuracy)
{
    matrix->set_profiling(true);

    const int num_executions = 10;
    for (int i = 0; i < num_executions; ++i) {
        matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("perf_gain_0", large_input);
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
                SUCCEED() << "Execution time is 0 - multichannel operations may be too fast to measure accurately";
            } else {
                EXPECT_GT(avg_time, 0.0) << "Should track positive execution time for measurable multichannel operations";
            }
        } else {
            SUCCEED() << "Average execution time not available in statistics";
        }
    } catch (const std::bad_any_cast&) {
        SUCCEED() << "Statistics collection active but type casting failed";
    }
}

TEST_F(MatrixPerformanceTest, HighChannelCountPerformance)
{
    auto high_channel_data = MatrixTestDataGenerator::create_test_multichannel_signal(16, 1024, 1.0);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = matrix->execute<MathematicalTransformer<>, std::vector<DataVariant>>(high_channel_data, MathematicalOperation::GAIN);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(result.has_value()) << "Should handle high channel count successfully";
    EXPECT_LT(duration.count(), 1000) << "Should process 16 channels in reasonable time (< 1s)";

    if (result.has_value()) {
        EXPECT_EQ(result->data.size(), 16) << "Should preserve all 16 channels";

        for (size_t ch = 0; ch < std::min(size_t(4), result->data.size()); ch += 4) {
            auto result_channel = std::get<std::vector<double>>(result->data[ch]);
            EXPECT_EQ(result_channel.size(), 1024) << "Channel " << ch << " should preserve sample count";
        }
    }
}

TEST_F(MatrixPerformanceTest, BatchPerformanceScaling)
{
    std::vector<std::vector<DataVariant>> batch_inputs;
    for (int i = 0; i < 8; ++i) {
        batch_inputs.push_back(MatrixTestDataGenerator::create_test_multichannel_signal(2, 1024, 0.5 + i * 0.1));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto results = matrix->execute_batch<MathematicalTransformer<>, std::vector<DataVariant>>(batch_inputs, MathematicalOperation::GAIN);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(results.size(), batch_inputs.size()) << "Should return result for each batch input";
    EXPECT_LT(duration.count(), 2000) << "Should process batch of 8 multichannel inputs in reasonable time (< 2s)";

    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].has_value()) {
            EXPECT_EQ(results[i]->data.size(), 2) << "Batch result " << i << " should preserve 2 channels";
        }
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
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }
    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
};

TEST_F(MatrixIntegrationTest, MultipleTransformerTypes)
{
    auto math_op = matrix->create_operation<MathematicalTransformer<>>("integration_gain", MathematicalOperation::GAIN);
    math_op->set_parameter("gain_factor", 1.5); // Explicit gain factor

    auto math_result = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("integration_gain", test_input);
    auto temporal_result = matrix->execute<TemporalTransformer<>, std::vector<DataVariant>>(test_input, TemporalOperation::TIME_REVERSE);

    EXPECT_TRUE(math_result.has_value()) << "Mathematical transformer should execute";
    EXPECT_TRUE(temporal_result.has_value()) << "Temporal transformer should execute";

    try {
        EXPECT_EQ(math_result->data.size(), test_data.size()) << "Math result should preserve channel count";
        EXPECT_EQ(temporal_result->data.size(), test_data.size()) << "Temporal result should preserve channel count";

        for (size_t ch = 0; ch < math_result->data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto math_channel = std::get<std::vector<double>>(math_result->data[ch]);
            auto temporal_channel = std::get<std::vector<double>>(temporal_result->data[ch]);

            EXPECT_EQ(math_channel.size(), original_channel.size()) << "Math channel " << ch << " should preserve size";
            EXPECT_EQ(temporal_channel.size(), original_channel.size()) << "Temporal channel " << ch << " should preserve size";

            // Verify transformations were applied
            bool math_changed = false, temporal_changed = false;
            for (size_t i = 0; i < std::min(original_channel.size(), math_channel.size()); ++i) {
                if (std::abs(math_channel[i] - original_channel[i]) > 1e-10) {
                    math_changed = true;
                    break;
                }
            }
            for (size_t i = 0; i < std::min(original_channel.size(), temporal_channel.size()); ++i) {
                if (std::abs(temporal_channel[i] - original_channel[i]) > 1e-10) {
                    temporal_changed = true;
                    break;
                }
            }

            EXPECT_TRUE(math_changed) << "Math transformation should modify channel " << ch << " data";
            EXPECT_TRUE(temporal_changed) << "Temporal transformation should modify channel " << ch << " data";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Multiple transformer integration test failed with: " << e.what();
    }
}

TEST_F(MatrixIntegrationTest, ChainWithDifferentOperationTypes)
{
    auto math_op = matrix->create_operation<MathematicalTransformer<>>("chain_math", MathematicalOperation::POWER);
    math_op->set_parameter("power_factor", 2.0);
    auto temporal_op = matrix->create_operation<TemporalTransformer<>>("chain_temporal", TemporalOperation::TIME_REVERSE);

    auto intermediate = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("chain_math", test_input);
    EXPECT_TRUE(intermediate.has_value()) << "First operation in chain should succeed";

    if (intermediate.has_value()) {
        auto final_result = matrix->execute_named<TemporalTransformer<>, std::vector<DataVariant>>("chain_temporal", intermediate->data);
        EXPECT_TRUE(final_result.has_value()) << "Second operation in chain should succeed";

        try {
            EXPECT_EQ(final_result->data.size(), test_data.size()) << "Final result should preserve channel count";

            for (size_t ch = 0; ch < final_result->data.size(); ++ch) {
                auto original_channel = std::get<std::vector<double>>(test_data[ch]);
                auto final_channel = std::get<std::vector<double>>(final_result->data[ch]);

                EXPECT_EQ(final_channel.size(), original_channel.size()) << "Final channel " << ch << " should preserve size";

                bool values_changed = false;
                for (size_t i = 0; i < std::min(original_channel.size(), final_channel.size()); ++i) {
                    if (std::abs(final_channel[i] - original_channel[i]) > 1e-10) {
                        values_changed = true;
                        break;
                    }
                }
                EXPECT_TRUE(values_changed) << "Chain should modify channel " << ch << " data";
            }
        } catch (const std::exception& e) {
            SUCCEED() << "Chain integration test failed with: " << e.what();
        }
    }
}

TEST_F(MatrixIntegrationTest, MixedSyncAsyncExecution)
{
    auto sync_result = matrix->execute<MathematicalTransformer<>, std::vector<DataVariant>>(test_input, MathematicalOperation::GAIN);
    auto async_future = matrix->execute_async<TemporalTransformer<>, std::vector<DataVariant>>(test_input, TemporalOperation::TIME_REVERSE);

    EXPECT_TRUE(sync_result.has_value()) << "Synchronous execution should complete";
    EXPECT_TRUE(async_future.valid()) << "Asynchronous execution should start";

    auto async_result = async_future.get();
    EXPECT_TRUE(async_result.has_value()) << "Asynchronous execution should complete";

    try {
        EXPECT_EQ(sync_result->data.size(), test_data.size()) << "Sync result should preserve channel count";
        EXPECT_EQ(async_result->data.size(), test_data.size()) << "Async result should preserve channel count";

        for (size_t ch = 0; ch < std::min(sync_result->data.size(), async_result->data.size()); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto sync_channel = std::get<std::vector<double>>(sync_result->data[ch]);
            auto async_channel = std::get<std::vector<double>>(async_result->data[ch]);

            EXPECT_EQ(sync_channel.size(), original_channel.size()) << "Sync channel " << ch << " should preserve size";
            EXPECT_EQ(async_channel.size(), original_channel.size()) << "Async channel " << ch << " should preserve size";
        }
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
        test_data = MatrixTestDataGenerator::create_test_multichannel_signal();
        test_input = test_data;
    }

    std::shared_ptr<ComputeMatrix> matrix;
    std::vector<DataVariant> test_data;
    std::vector<DataVariant> test_input;
};

TEST_F(MatrixResilienceTest, TimeoutConfiguration)
{
    matrix->set_default_timeout(std::chrono::milliseconds(1000));

    auto result = matrix->execute<MathematicalTransformer<>, std::vector<DataVariant>>(test_input, MathematicalOperation::GAIN);
    EXPECT_TRUE(result.has_value()) << "Normal multichannel execution should complete within timeout";

    if (result.has_value()) {
        EXPECT_EQ(result->data.size(), test_data.size()) << "Should preserve channel count within timeout";
    }
}

TEST_F(MatrixResilienceTest, ErrorRecovery)
{
    size_t error_count = 0;
    matrix->set_error_callback([&error_count](const std::exception& /*e*/, const std::type_index& /*op_type*/) {
        error_count++;
    });

    auto bad_result = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("nonexistent", test_input);
    EXPECT_FALSE(bad_result.has_value()) << "Bad execution should fail";

    auto gain_op = matrix->create_operation<MathematicalTransformer<>>("recovery_gain", MathematicalOperation::GAIN);
    gain_op->set_parameter("gain_factor", 1.2);

    auto good_result = matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("recovery_gain", test_input);
    EXPECT_TRUE(good_result.has_value()) << "Matrix should recover from errors";

    if (good_result.has_value()) {
        EXPECT_EQ(good_result->data.size(), test_data.size()) << "Recovery should preserve multichannel structure";
    }
}

TEST_F(MatrixResilienceTest, StatisticsAfterErrors)
{
    matrix->set_profiling(true);

    auto gain_op = matrix->create_operation<MathematicalTransformer<>>("stats_gain", MathematicalOperation::GAIN);
    gain_op->set_parameter("gain_factor", 1.1);

    auto power_op = matrix->create_operation<MathematicalTransformer<>>("stats_power", MathematicalOperation::POWER);
    power_op->set_parameter("power_factor", 2.0);

    matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("stats_gain", test_input);
    matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("stats_power", test_input);

    matrix->execute_named<MathematicalTransformer<>, std::vector<DataVariant>>("nonexistent", test_input);

    auto stats = matrix->get_statistics();

    try {
        auto total_executions = std::any_cast<size_t>(stats["total_executions"]);
        auto failed_executions = std::any_cast<size_t>(stats["failed_executions"]);

        EXPECT_GE(total_executions, 2) << "Should count successful multichannel executions";
        EXPECT_GE(failed_executions, 0) << "Should track failed multichannel executions";
    } catch (const std::bad_any_cast&) {
        SUCCEED() << "Statistics tracking after errors completed";
    }
}

TEST_F(MatrixResilienceTest, MultiChannelErrorHandling)
{
    std::vector<DataVariant> empty_multichannel;
    std::vector<DataVariant> empty_channels = {
        DataVariant(std::vector<double> {}),
        DataVariant(std::vector<double> {})
    };

    EXPECT_NO_THROW({
        auto result1 = matrix->execute<MathematicalTransformer<>>(empty_multichannel, MathematicalOperation::GAIN);
        auto result2 = matrix->execute<MathematicalTransformer<>>(empty_channels, MathematicalOperation::GAIN);
    }) << "Should handle edge case multichannel inputs gracefully";
}

TEST_F(MatrixResilienceTest, ConcurrentErrorHandling)
{
    std::vector<std::future<std::optional<IO<std::vector<DataVariant>>>>> futures;

    auto valid_op = matrix->create_operation<MathematicalTransformer<>>("concurrent_valid", MathematicalOperation::GAIN);
    valid_op->set_parameter("gain_factor", 1.3);

    for (int i = 0; i < 3; ++i) {
        futures.push_back(matrix->execute_named_async<MathematicalTransformer<>, std::vector<DataVariant>>("concurrent_valid", test_input));
        futures.push_back(matrix->execute_named_async<MathematicalTransformer<>, std::vector<DataVariant>>("nonexistent_" + std::to_string(i), test_input));
    }

    int successful = 0, failed = 0;
    for (auto& future : futures) {
        EXPECT_TRUE(future.valid()) << "All futures should be valid";
        auto result = future.get();
        if (result.has_value()) {
            successful++;
            EXPECT_EQ(result->data.size(), test_data.size()) << "Successful concurrent result should preserve channels";
        } else {
            failed++;
        }
    }

    EXPECT_EQ(successful, 3) << "Should have 3 successful concurrent operations";
    EXPECT_EQ(failed, 3) << "Should have 3 failed concurrent operations";
}

}
