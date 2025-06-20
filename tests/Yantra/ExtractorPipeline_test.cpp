#include "../mock_signalsourcecontainer.hpp"

#include "MayaFlux/Yantra/Extractors/ExtractorPipeline.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class FeatureExtractorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data.resize(1024);
        for (size_t i = 0; i < test_data.size(); ++i) {
            test_data[i] = std::sin(2.0 * M_PI * i / 64.0) * 0.5; // 16 cycles
        }

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);
        extractor = std::make_shared<FeatureExtractor>();
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::shared_ptr<FeatureExtractor> extractor;
};

TEST_F(FeatureExtractorTest, ExtractMeanFeature)
{
    extractor->set_extraction_method("mean");

    ExtractorInput input { DataVariant { test_data } };
    auto result = extractor->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    const auto& values = std::get<std::vector<double>>(result.base_output);

    EXPECT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], 0.0, 1e-10); // Sine wave mean should be ~0
}

TEST_F(FeatureExtractorTest, ExtractEnergyFeature)
{
    extractor->set_extraction_method("energy");

    ExtractorInput input { DataVariant { test_data } };
    auto result = extractor->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    const auto& values = std::get<std::vector<double>>(result.base_output);

    EXPECT_EQ(values.size(), 1);
    EXPECT_GT(values[0], 0.0);
    EXPECT_NEAR(values[0], 0.125, 0.01); // Energy of 0.5 amplitude sine ~= 0.125
}

TEST_F(FeatureExtractorTest, TypedExtractionInterface)
{
    auto result = extractor->extract_typed<DataVariant, std::vector<double>>(
        DataVariant { test_data }, "mean");

    EXPECT_EQ(result.size(), 1);
    EXPECT_NEAR(result[0], 0.0, 1e-10);
}

TEST_F(FeatureExtractorTest, ContainerInputExtraction)
{
    extractor->set_extraction_method("energy");

    auto foo = container->get_processed_data();

    // ExtractorInput input { std::static_pointer_cast<Kakshya::SignalSourceContainer>(container) };
    ExtractorInput input { foo };
    auto result = extractor->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    const auto& values = std::get<std::vector<double>>(result.base_output);

    EXPECT_FALSE(values.empty());
    EXPECT_GT(values[0], 0.0);
}

TEST_F(FeatureExtractorTest, AvailableMethodsQuery)
{
    auto methods = extractor->get_available_methods();

    EXPECT_FALSE(methods.empty());
    EXPECT_TRUE(std::find(methods.begin(), methods.end(), "mean") != methods.end());
    EXPECT_TRUE(std::find(methods.begin(), methods.end(), "energy") != methods.end());
    EXPECT_TRUE(std::find(methods.begin(), methods.end(), "variance") != methods.end());
}

TEST_F(FeatureExtractorTest, InvalidMethodThrows)
{
    extractor->set_extraction_method("invalid_method");

    ExtractorInput input { DataVariant { test_data } };
    // Should not throw for unsupported method, but return default behavior
    EXPECT_NO_THROW(extractor->apply_operation(input));
}

TEST_F(FeatureExtractorTest, EmptyDataHandling)
{
    std::vector<double> empty_data;
    ExtractorInput input { DataVariant { empty_data } };

    EXPECT_NO_THROW(extractor->apply_operation(input));
}

class ExtractorNodeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_result = std::vector<double> { 1.0, 2.0, 3.0 };
        concrete_node = std::make_shared<ConcreteExtractorNode<std::vector<double>>>(test_result);

        lazy_func = [this]() {
            return ExtractorOutput { std::vector<double> { 4.0, 5.0, 6.0 } };
        };
        lazy_node = std::make_shared<LazyExtractorNode>(lazy_func);
    }

    std::vector<double> test_result;
    std::shared_ptr<ConcreteExtractorNode<std::vector<double>>> concrete_node;
    std::function<ExtractorOutput()> lazy_func;
    std::shared_ptr<LazyExtractorNode> lazy_node;
};

TEST_F(ExtractorNodeTest, ConcreteNodeExtraction)
{
    auto result = concrete_node->extract();

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    const auto& values = std::get<std::vector<double>>(result.base_output);

    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1.0);
    EXPECT_EQ(values[1], 2.0);
    EXPECT_EQ(values[2], 3.0);
}

TEST_F(ExtractorNodeTest, LazyNodeEvaluation)
{
    EXPECT_TRUE(lazy_node->is_lazy());

    // First extraction should evaluate the function
    auto result1 = lazy_node->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result1.base_output));

    // Second extraction should return cached result
    auto result2 = lazy_node->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result2.base_output));

    const auto& values1 = std::get<std::vector<double>>(result1.base_output);
    const auto& values2 = std::get<std::vector<double>>(result2.base_output);

    EXPECT_EQ(values1, values2); // Should be same (cached)
}

TEST_F(ExtractorNodeTest, TypedGetAs)
{
    auto typed_result = concrete_node->get_as<std::vector<double>>();

    ASSERT_TRUE(typed_result.has_value());
    EXPECT_EQ(typed_result->size(), 3);
    EXPECT_EQ((*typed_result)[0], 1.0);
}

TEST_F(ExtractorNodeTest, NodeTypeNames)
{
    EXPECT_FALSE(concrete_node->get_type_name().empty());
    EXPECT_EQ(lazy_node->get_type_name(), "LazyExtractorNode");
}

// ===== ExtractorChain Tests =====

class ExtractorChainTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data.resize(512);
        std::iota(test_data.begin(), test_data.end(), 1.0); // 1, 2, 3, ...

        chain = std::make_shared<ExtractorChain>();

        mean_extractor = std::make_shared<FeatureExtractor>();
        mean_extractor->set_extraction_method("mean");

        energy_extractor = std::make_shared<FeatureExtractor>();
        energy_extractor->set_extraction_method("energy");
    }

    std::vector<double> test_data;
    std::shared_ptr<ExtractorChain> chain;
    std::shared_ptr<FeatureExtractor> mean_extractor;
    std::shared_ptr<FeatureExtractor> energy_extractor;
};

TEST_F(ExtractorChainTest, SingleExtractorChain)
{
    chain->add_extractor(mean_extractor, "mean_calc");

    ExtractorInput input { DataVariant { test_data } };
    auto result = chain->extract(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    const auto& values = std::get<std::vector<double>>(result.base_output);

    EXPECT_EQ(values.size(), 1);
    EXPECT_NEAR(values[0], 256.5, 0.1); // Mean of 1..512
}

TEST_F(ExtractorChainTest, MultipleExtractorChain)
{
    chain->add_extractor(mean_extractor, "mean_calc");
    chain->add_extractor(energy_extractor, "energy_calc");

    ExtractorInput input { DataVariant { test_data } };
    auto result = chain->extract(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    // Should process mean first, then energy of the mean result
}

TEST_F(ExtractorChainTest, ChainNameRetrieval)
{
    chain->add_extractor(mean_extractor, "mean_calc");
    chain->add_extractor(energy_extractor, "energy_calc");

    auto names = chain->get_extractor_names();

    EXPECT_EQ(names.size(), 2);
    EXPECT_EQ(names[0], "mean_calc");
    EXPECT_EQ(names[1], "energy_calc");
}

TEST_F(ExtractorChainTest, EmptyChainThrows)
{
    ExtractorInput input { DataVariant { test_data } };

    EXPECT_THROW(chain->extract(input), std::runtime_error);
}

TEST_F(ExtractorChainTest, UnnamedExtractorHandling)
{
    chain->add_extractor(mean_extractor); // No name provided

    auto names = chain->get_extractor_names();

    EXPECT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "unnamed");
}

// ===== ExtractionPipeline Tests =====

class ExtractionPipelineTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data.resize(256);
        for (size_t i = 0; i < test_data.size(); ++i) {
            test_data[i] = std::sin(2.0 * M_PI * i / 32.0); // 8 cycles
        }

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        pipeline = std::make_shared<ExtractionPipeline>();
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::shared_ptr<ExtractionPipeline> pipeline;
};

TEST_F(ExtractionPipelineTest, TemplatedExtractorAddition)
{
    pipeline->add_extractor<FeatureExtractor>("feature_stage");

    auto stages = pipeline->get_pipeline_stages();
    EXPECT_EQ(stages.size(), 1);
    EXPECT_EQ(stages[0], "feature_stage");
}

TEST_F(ExtractionPipelineTest, DataVariantProcessing)
{
    pipeline->add_extractor<FeatureExtractor>("features");

    auto result = pipeline->process(DataVariant { test_data });

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractionPipelineTest, ContainerProcessing)
{
    pipeline->add_extractor<FeatureExtractor>("features");

    auto result = pipeline->process(container->get_processed_data());

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractionPipelineTest, ExtractorInputProcessing)
{
    pipeline->add_extractor<FeatureExtractor>("features");

    ExtractorInput input { DataVariant { test_data } };
    auto result = pipeline->process(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractionPipelineTest, CustomExtractorAddition)
{
    auto custom_extractor = std::make_shared<FeatureExtractor>();
    custom_extractor->set_extraction_method("energy");

    pipeline->add_custom_extractor(custom_extractor, "custom_energy");

    auto stages = pipeline->get_pipeline_stages();
    EXPECT_EQ(stages.size(), 1);
    EXPECT_EQ(stages[0], "custom_energy");
}

// ===== ExtractionGrammar Tests =====

class ExtractionGrammarTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = std::vector<double> { 0.1, 0.5, 0.9, 0.3, 0.7 };
        grammar = std::make_shared<ExtractionGrammar>();

        // Add test rules
        ExtractionGrammar::Rule peak_rule {
            "find_peaks",
            [](const ExtractorInput& input) -> bool {
                // Always matches for testing
                return std::holds_alternative<DataVariant>(input.base_input);
            },
            [](const ExtractorInput& input) -> ExtractorOutput {
                return ExtractorOutput { std::vector<double> { 0.9, 0.7 } }; // Mock peaks
            },
            {},
            ExtractionGrammar::ExtractionContext::TEMPORAL,
            10 // High priority
        };

        ExtractionGrammar::Rule energy_rule {
            "calculate_energy",
            [](const ExtractorInput& input) -> bool {
                return std::holds_alternative<DataVariant>(input.base_input);
            },
            [](const ExtractorInput& input) -> ExtractorOutput {
                return ExtractorOutput { std::vector<double> { 1.59 } }; // Mock energy
            },
            { "find_peaks" },
            ExtractionGrammar::ExtractionContext::SPECTRAL,
            5 // Lower priority
        };

        grammar->add_rule(peak_rule);
        grammar->add_rule(energy_rule);
    }

    std::vector<double> test_data;
    std::shared_ptr<ExtractionGrammar> grammar;
};

TEST_F(ExtractionGrammarTest, RuleAdditionAndRetrieval)
{
    auto available_rules = grammar->get_available_rules();

    EXPECT_EQ(available_rules.size(), 2);
    EXPECT_TRUE(std::find(available_rules.begin(), available_rules.end(), "find_peaks") != available_rules.end());
    EXPECT_TRUE(std::find(available_rules.begin(), available_rules.end(), "calculate_energy") != available_rules.end());
}

TEST_F(ExtractionGrammarTest, SpecificRuleExtraction)
{
    ExtractorInput input { DataVariant { test_data } };

    auto result = grammar->extract_by_rule("find_peaks", input);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result->base_output));

    const auto& values = std::get<std::vector<double>>(result->base_output);
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 0.9);
    EXPECT_EQ(values[1], 0.7);
}

TEST_F(ExtractionGrammarTest, NonMatchingRuleReturnsNullopt)
{
    // Create input that doesn't match any rules
    ExtractorInput input { Region { { 0 }, { 100 }, {} } };

    auto result = grammar->extract_by_rule("find_peaks", input);

    EXPECT_FALSE(result.has_value());
}

TEST_F(ExtractionGrammarTest, ExtractAllMatchingRules)
{
    ExtractorInput input { DataVariant { test_data } };

    auto results = grammar->extract_all_matching(input);

    EXPECT_EQ(results.size(), 2); // Both rules should match
}

TEST_F(ExtractionGrammarTest, NonExistentRuleReturnsNullopt)
{
    ExtractorInput input { DataVariant { test_data } };

    auto result = grammar->extract_by_rule("non_existent_rule", input);

    EXPECT_FALSE(result.has_value());
}

// ===== ExtractorInput/Output Tests =====

class ExtractorIOTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = std::vector<double> { 1.0, 2.0, 3.0 };
        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
};

TEST_F(ExtractorIOTest, ExtractorInputConstruction)
{
    // Test various constructor forms
    ExtractorInput input1 { DataVariant { test_data } };
    ExtractorInput input2 { std::static_pointer_cast<Kakshya::SignalSourceContainer>(container) };
    ExtractorInput input3 { Region { { 0 }, { 100 }, {} } };

    EXPECT_TRUE(std::holds_alternative<DataVariant>(input1.base_input));
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<SignalSourceContainer>>(input2.base_input));
    EXPECT_TRUE(std::holds_alternative<Region>(input3.base_input));
}

TEST_F(ExtractorIOTest, ExtractorOutputConstruction)
{
    // Test various constructor forms
    ExtractorOutput output1 { test_data };
    ExtractorOutput output2 { DataVariant { test_data } };
    ExtractorOutput output3 { RegionGroup { "test_group" } };

    EXPECT_TRUE(std::holds_alternative<std::vector<double>>(output1.base_output));
    EXPECT_TRUE(std::holds_alternative<DataVariant>(output2.base_output));
    EXPECT_TRUE(std::holds_alternative<RegionGroup>(output3.base_output));
}

TEST_F(ExtractorIOTest, RecursiveInputSupport)
{
    ExtractorInput input { DataVariant { test_data } };

    auto node = std::make_shared<ConcreteExtractorNode<std::vector<double>>>(test_data);
    input.add_recursive_input(node);

    EXPECT_TRUE(input.has_recursive_inputs());
    EXPECT_EQ(input.recursive_inputs.size(), 1);
}

TEST_F(ExtractorIOTest, RecursiveOutputSupport)
{
    ExtractorOutput output { test_data };

    auto node = std::make_shared<ConcreteExtractorNode<std::vector<double>>>(test_data);
    output.add_recursive_output(node);

    EXPECT_TRUE(output.has_recursive_outputs());
    EXPECT_EQ(output.recursive_outputs.size(), 1);
}

// ===== Integration Tests =====

class ExtractorIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create complex test signal
        test_data.resize(1024);
        for (size_t i = 0; i < test_data.size(); ++i) {
            double t = static_cast<double>(i) / 1024.0;
            test_data[i] = 0.5 * std::sin(2.0 * M_PI * 5 * t) + // 5 Hz component
                0.3 * std::sin(2.0 * M_PI * 15 * t) + // 15 Hz component
                0.1 * (static_cast<double>(rand()) / RAND_MAX - 0.5); // Noise
        }

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
};

TEST_F(ExtractorIntegrationTest, PipelineWithMultipleStages)
{
    ExtractionPipeline pipeline;
    pipeline.add_extractor<FeatureExtractor>("feature_stage");

    auto result = pipeline.process(container->get_processed_data());

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    const auto& values = std::get<std::vector<double>>(result.base_output);
    EXPECT_FALSE(values.empty());
}

TEST_F(ExtractorIntegrationTest, ChainedExtractionWithRecursion)
{
    auto extractor = std::make_shared<FeatureExtractor>();
    extractor->set_extraction_method("mean");

    auto base_node = extractor->create_node(test_data);
    auto recursive_node = extractor->create_recursive_node(base_node);

    auto result = recursive_node->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractorIntegrationTest, LazyEvaluationInChain)
{
    auto extractor = std::make_shared<FeatureExtractor>();

    auto lazy_node = extractor->create_lazy_node([this]() {
        return ExtractorOutput { test_data };
    });

    EXPECT_TRUE(lazy_node->is_lazy());

    auto result = lazy_node->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

// ===== Error Handling Tests =====

class ExtractorErrorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        extractor = std::make_shared<FeatureExtractor>();
    }

    std::shared_ptr<FeatureExtractor> extractor;
};

/* TEST_F(ExtractorErrorTest, TypeMismatchInTypedExtraction)
{
    std::vector<double> test_data { 1.0, 2.0, 3.0 };

    // Try to extract wrong output type
    EXPECT_THROW(
        extractor->extract_typed<DataVariant, std::vector<float>>(
            DataVariant { test_data }, "mean"),
        std::runtime_error);
} */

TEST_F(ExtractorErrorTest, EmptyChainExtraction)
{
    ExtractorChain chain;
    ExtractorInput input { DataVariant { std::vector<double> { 1.0, 2.0 } } };

    EXPECT_THROW(chain.extract(input), std::runtime_error);
}

TEST_F(ExtractorErrorTest, InvalidParameterAccess)
{
    EXPECT_NO_THROW(extractor->get_parameter("non_existent"));
}

} // namespace MayaFlux::Test
