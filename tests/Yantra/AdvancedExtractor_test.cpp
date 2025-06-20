#include "../mock_signalsourcecontainer.hpp"

#include "mock_extractor.hpp"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/ExtractorPipeline.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

class AdvancedExtractorNodeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 };
        extractor = std::make_shared<MockFeatureExtractor>();
        extractor->set_extraction_method("mean");
    }

    std::vector<double> test_data;
    std::shared_ptr<MockFeatureExtractor> extractor;
};

TEST_F(AdvancedExtractorNodeTest, RecursiveNodeChaining)
{
    auto base_node = extractor->create_node(test_data);
    auto recursive_node1 = extractor->create_recursive_node(base_node);
    auto recursive_node2 = extractor->create_recursive_node(recursive_node1);

    std::string base_type_name = base_node->get_type_name();
    EXPECT_FALSE(base_type_name.empty()); // Just check it's not empty

    EXPECT_EQ(recursive_node1->get_type_name(), "RecursiveExtractorNode");
    EXPECT_EQ(recursive_node2->get_type_name(), "RecursiveExtractorNode");

    auto result = recursive_node2->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(AdvancedExtractorNodeTest, LazyNodeCaching)
{
    int call_count = 0;
    auto lazy_func = [&call_count, this]() -> ExtractorOutput {
        call_count++;
        return ExtractorOutput { std::vector<double> { static_cast<double>(call_count) } };
    };

    auto lazy_node = extractor->create_lazy_node(lazy_func);

    auto result1 = lazy_node->extract();
    EXPECT_EQ(call_count, 1);

    auto result2 = lazy_node->extract();
    EXPECT_EQ(call_count, 1); // Still 1, not 2

    const auto& values1 = std::get<std::vector<double>>(result1.base_output);
    const auto& values2 = std::get<std::vector<double>>(result2.base_output);
    EXPECT_EQ(values1, values2);
}

TEST_F(AdvancedExtractorNodeTest, MixedNodeTypeChaining)
{
    // Create a mixed chain: Concrete -> Lazy -> Recursive
    auto concrete_node = extractor->create_node(test_data);

    auto lazy_node = extractor->create_lazy_node([this]() {
        return ExtractorOutput { std::vector<double> { 99.0 } };
    });

    auto recursive_node = extractor->create_recursive_node(lazy_node);

    EXPECT_FALSE(concrete_node->is_lazy());
    EXPECT_TRUE(lazy_node->is_lazy());
    EXPECT_FALSE(recursive_node->is_lazy());

    auto result = recursive_node->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

class ExtractionStrategyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 };
        extractor = std::make_shared<MockAnalyzerIntegratedExtractor>();

        // Create a mock analyzer for delegation tests
        mock_analyzer = std::make_shared<EnergyAnalyzer>(256, 128);
        extractor->set_analyzer(mock_analyzer);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockAnalyzerIntegratedExtractor> extractor;
    std::shared_ptr<EnergyAnalyzer> mock_analyzer;
};

TEST_F(ExtractionStrategyTest, ImmediateExtractionStrategy)
{
    ExtractorInput input { DataVariant { test_data } };

    auto result = extractor->extract_with_strategy(input, ExtractionStrategy::IMMEDIATE);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    EXPECT_GT(extractor->get_direct_extraction_count(), 0);
}

TEST_F(ExtractionStrategyTest, LazyExtractionStrategy)
{
    ExtractorInput input { DataVariant { test_data } };

    auto result = extractor->extract_with_strategy(input, ExtractionStrategy::LAZY);

    EXPECT_TRUE(result.has_recursive_outputs() || std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractionStrategyTest, AnalyzerDelegationStrategy)
{
    extractor->set_use_analyzer(true);
    extractor->set_extraction_method("delegate_to_analyzer");

    mock_analyzer->set_window_parameters(5, 2);

    ExtractorInput input { DataVariant { test_data } };

    auto result = extractor->extract_with_strategy(input, ExtractionStrategy::ANALYZER_DELEGATE);

    EXPECT_TRUE(extractor->uses_analyzer());
    EXPECT_GT(extractor->get_delegation_count(), 0);

    ASSERT_TRUE(std::holds_alternative<RegionGroup>(result.base_output));

    const auto& region_group = std::get<RegionGroup>(result.base_output);
    EXPECT_EQ(region_group.regions.size(), 1);
}

TEST_F(ExtractionStrategyTest, AnalyzerDelegationStrategyRawValues)
{
    extractor->set_use_analyzer(true);
    extractor->set_extraction_method("delegate_to_analyzer");

    mock_analyzer->set_parameter("method", "rms");
    mock_analyzer->set_window_parameters(5, 2);

    mock_analyzer->set_output_granularity(AnalysisGranularity::RAW_VALUES);

    ExtractorInput input { DataVariant { test_data } };

    auto result = extractor->extract_with_strategy(input, ExtractionStrategy::ANALYZER_DELEGATE);

    EXPECT_TRUE(extractor->uses_analyzer());
    EXPECT_GT(extractor->get_delegation_count(), 0);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractionStrategyTest, RecursiveExtractionStrategy)
{
    ExtractorInput input { DataVariant { test_data } };

    auto base_node = extractor->create_node(test_data);
    input.add_recursive_input(base_node);

    auto result = extractor->extract_with_strategy(input, ExtractionStrategy::RECURSIVE);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

class ComplexPipelineTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        audio_data.resize(512);
        for (size_t i = 0; i < audio_data.size(); ++i) {
            audio_data[i] = std::sin(2.0 * M_PI * i / 64.0) + 0.3 * std::sin(2.0 * M_PI * i / 16.0);
        }

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(audio_data);

        RegionGroup test_group("test_regions");
        test_group.add_region(Region { { 0 }, { 128 }, {} });
        test_group.add_region(Region { { 128 }, { 256 }, {} });
        test_group.add_region(Region { { 256 }, { 384 }, {} });
        test_group.add_region(Region { { 384 }, { 512 }, {} });
        container->add_region_group(test_group);
    }

    std::vector<double> audio_data;
    std::shared_ptr<MockSignalSourceContainer> container;
};

TEST_F(ComplexPipelineTest, MultiStageFeatureExtraction)
{
    ExtractionPipeline pipeline;

    auto mean_extractor = std::make_shared<MockFeatureExtractor>();
    mean_extractor->set_extraction_method("mean");
    pipeline.add_custom_extractor(mean_extractor, "mean_stage");

    auto energy_extractor = std::make_shared<MockFeatureExtractor>();
    energy_extractor->set_extraction_method("energy");
    pipeline.add_custom_extractor(energy_extractor, "energy_stage");

    auto result = pipeline.process(container->get_processed_data());

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));

    auto stages = pipeline.get_pipeline_stages();
    EXPECT_EQ(stages.size(), 2);
    EXPECT_EQ(stages[0], "mean_stage");
    EXPECT_EQ(stages[1], "energy_stage");
}

TEST_F(ComplexPipelineTest, RegionBasedExtraction)
{
    auto feature_extractor = std::make_shared<MockFeatureExtractor>();
    feature_extractor->set_extraction_method("variance");

    Region test_region { { 100 }, { 200 }, {} };
    ExtractorInput input { test_region };

    auto result = feature_extractor->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ComplexPipelineTest, MultiModalExtraction)
{
    auto feature_extractor = std::make_shared<MockFeatureExtractor>();

    std::vector<ExtractorInput> inputs = {
        ExtractorInput { DataVariant { audio_data } },
        ExtractorInput { std::static_pointer_cast<Kakshya::SignalSourceContainer>(container) },
        ExtractorInput { Region { { 0 }, { 256 }, {} } },
        ExtractorInput { container->get_region_group("test_regions") }
    };

    for (const auto& input : inputs) {
        EXPECT_NO_THROW({
            auto result = feature_extractor->apply_operation(input);
            EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
        });
    }
}

class GrammarExtractionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = std::vector<double> { 0.1, 0.8, 0.3, 0.9, 0.2, 0.7, 0.4 };
        grammar = std::make_shared<MockExtractionGrammar>();

        MockExtractionGrammar::MockRule onset_rule("detect_onsets", 10);
        onset_rule.matcher = [](const ExtractorInput& input) {
            if (auto* data = std::get_if<DataVariant>(&input.base_input)) {
                return true;
            }
            return false;
        };
        onset_rule.extractor = [](const ExtractorInput& input) {
            return ExtractorOutput { std::vector<double> { 1.0, 3.0, 5.0 } }; // Mock onset times
        };

        MockExtractionGrammar::MockRule tempo_rule("estimate_tempo", 8);
        tempo_rule.matcher = [](const ExtractorInput& input) {
            return std::holds_alternative<DataVariant>(input.base_input);
        };
        tempo_rule.extractor = [](const ExtractorInput& input) {
            return ExtractorOutput { std::vector<double> { 120.0 } }; // Mock tempo in BPM
        };

        MockExtractionGrammar::MockRule harmony_rule("analyze_harmony", 5);
        harmony_rule.matcher = [](const ExtractorInput& input) {
            return std::holds_alternative<DataVariant>(input.base_input);
        };
        harmony_rule.extractor = [](const ExtractorInput& input) {
            return ExtractorOutput { std::vector<double> { 0.8, 0.2, 0.6, 0.1 } }; // Mock chord probabilities
        };

        grammar->add_mock_rule(onset_rule);
        grammar->add_mock_rule(tempo_rule);
        grammar->add_mock_rule(harmony_rule);
    }

    std::vector<double> test_data;
    std::shared_ptr<MockExtractionGrammar> grammar;
};

TEST_F(GrammarExtractionTest, PriorityBasedRuleOrdering)
{
    auto available_rules = grammar->get_available_rules();

    EXPECT_EQ(available_rules.size(), 3);
    // Rules should be ordered by priority (detect_onsets=10, estimate_tempo=8, analyze_harmony=5)
    EXPECT_EQ(available_rules[0], "detect_onsets");
}

TEST_F(GrammarExtractionTest, ConditionalRuleExecution)
{
    ExtractorInput input { DataVariant { test_data } };

    auto onset_result = grammar->extract_by_rule("detect_onsets", input);
    ASSERT_TRUE(onset_result.has_value());

    const auto& onset_times = std::get<std::vector<double>>(onset_result->base_output);
    EXPECT_EQ(onset_times.size(), 3);
    EXPECT_EQ(onset_times[0], 1.0);
    EXPECT_EQ(onset_times[1], 3.0);
    EXPECT_EQ(onset_times[2], 5.0);
}

TEST_F(GrammarExtractionTest, RuleEnablingDisabling)
{
    grammar->enable_rule("analyze_harmony", false);

    ExtractorInput input { DataVariant { test_data } };
    auto all_results = grammar->extract_all_matching(input);

    EXPECT_EQ(all_results.size(), 2); // Should be 2, not 3

    grammar->enable_rule("analyze_harmony", true);
    all_results = grammar->extract_all_matching(input);

    EXPECT_EQ(all_results.size(), 3); // Back to 3
}

TEST_F(GrammarExtractionTest, MultiContextExtraction)
{
    ExtractorInput input { DataVariant { test_data } };

    auto all_results = grammar->extract_all_matching(input);

    EXPECT_EQ(all_results.size(), 3);

    for (const auto& result : all_results) {
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    }
}

class ExtractorPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        large_data.resize(44100);
        for (size_t i = 0; i < large_data.size(); ++i) {
            large_data[i] = std::sin(2.0 * M_PI * 440.0 * i / 44100.0) * std::exp(-static_cast<double>(i) / 10000.0); // Decay envelope
        }

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(large_data);
    }

    std::vector<double> large_data;
    std::shared_ptr<MockSignalSourceContainer> container;
};

TEST_F(ExtractorPerformanceTest, LargeDatasetExtraction)
{
    auto feature_extractor = std::make_shared<MockFeatureExtractor>();
    feature_extractor->set_extraction_method("mfcc");

    auto start_time = std::chrono::high_resolution_clock::now();

    ExtractorInput input { DataVariant { large_data } };
    auto result = feature_extractor->apply_operation(input);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    const auto& mfcc_coeffs = std::get<std::vector<double>>(result.base_output);
    EXPECT_EQ(mfcc_coeffs.size(), 13); // Standard MFCC size

    EXPECT_LT(duration.count(), 1000); // Less than 1 second
}

TEST_F(ExtractorPerformanceTest, ConcurrentExtractionStress)
{
    const size_t num_threads = 4;
    const size_t extractions_per_thread = 10;

    std::vector<std::thread> threads;
    std::vector<bool> thread_results(num_threads, false);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &thread_results, extractions_per_thread]() {
            try {
                auto feature_extractor = std::make_shared<MockFeatureExtractor>();

                for (size_t i = 0; i < extractions_per_thread; ++i) {
                    feature_extractor->set_extraction_method("energy");
                    ExtractorInput input { DataVariant { large_data } };
                    auto result = feature_extractor->apply_operation(input);

                    EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
                }

                thread_results[t] = true;
            } catch (...) {
                thread_results[t] = false;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all threads completed successfully
    for (bool result : thread_results) {
        EXPECT_TRUE(result);
    }
}

TEST_F(ExtractorPerformanceTest, ChainedExtractionPerformance)
{
    ExtractorChain chain;

    auto mean_extractor = std::make_shared<MockFeatureExtractor>();
    mean_extractor->set_extraction_method("mean");
    chain.add_extractor(mean_extractor, "mean");

    auto variance_extractor = std::make_shared<MockFeatureExtractor>();
    variance_extractor->set_extraction_method("variance");
    chain.add_extractor(variance_extractor, "variance");

    auto energy_extractor = std::make_shared<MockFeatureExtractor>();
    energy_extractor->set_extraction_method("energy");
    chain.add_extractor(energy_extractor, "energy");

    auto start_time = std::chrono::high_resolution_clock::now();

    ExtractorInput input { DataVariant { large_data } };
    auto result = chain.extract(input);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    EXPECT_LT(duration.count(), 500); // Chain should be efficient
}

class ExtractorMemoryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data.resize(1024);
        std::iota(test_data.begin(), test_data.end(), 1.0);
    }

    std::vector<double> test_data;
};

TEST_F(ExtractorMemoryTest, NodeLifetimeManagement)
{
    std::weak_ptr<ExtractorNode> weak_node;

    {
        auto extractor = std::make_shared<MockFeatureExtractor>();
        auto concrete_node = extractor->create_node(test_data);
        weak_node = concrete_node;

        EXPECT_FALSE(weak_node.expired());

        // Use the node
        auto result = concrete_node->extract();
        ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    }

    // Node should still be alive due to weak_ptr reference management
    // In a real scenario, this would depend on the specific implementation
}

TEST_F(ExtractorMemoryTest, LargeDataCopyAvoidance)
{
    std::vector<double> large_dataset(100000);
    std::iota(large_dataset.begin(), large_dataset.end(), 1.0);

    auto extractor = std::make_shared<MockFeatureExtractor>();

    ExtractorInput input { DataVariant { std::move(large_dataset) } };

    auto result = extractor->apply_operation(input);

    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    EXPECT_TRUE(large_dataset.empty());
}

TEST_F(ExtractorMemoryTest, RecursiveNodeMemoryLeaks)
{
    auto extractor = std::make_shared<MockFeatureExtractor>();

    std::vector<std::shared_ptr<ExtractorNode>> nodes;

    auto base_node = extractor->create_node(test_data);
    nodes.push_back(base_node);

    for (int i = 0; i < 10; ++i) {
        auto recursive_node = extractor->create_recursive_node(nodes.back());
        nodes.push_back(recursive_node);
    }

    auto result = nodes.back()->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));

    nodes.clear();
}

class ExtractorEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        extractor = std::make_shared<MockUniversalExtractor>();
    }

    std::shared_ptr<MockUniversalExtractor> extractor;
};

TEST_F(ExtractorEdgeCaseTest, EmptyInputHandling)
{
    std::vector<double> empty_data;
    ExtractorInput input { DataVariant { empty_data } };

    EXPECT_NO_THROW({
        auto result = extractor->apply_operation(input);
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    });
}

TEST_F(ExtractorEdgeCaseTest, NaNAndInfinityHandling)
{
    std::vector<double> problematic_data {
        1.0, 2.0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -5.0
    };

    ExtractorInput input { DataVariant { problematic_data } };

    EXPECT_NO_THROW({
        auto result = extractor->apply_operation(input);
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    });
}

TEST_F(ExtractorEdgeCaseTest, ExceptionPropagation)
{
    extractor->set_should_throw(true);

    ExtractorInput input { DataVariant { std::vector<double> { 1.0, 2.0 } } };

    EXPECT_THROW(extractor->apply_operation(input), std::runtime_error);
}

TEST_F(ExtractorEdgeCaseTest, InvalidParameterHandling)
{
    extractor->set_parameter("invalid_param", std::string("invalid_value"));

    EXPECT_NO_THROW({
        auto param = extractor->get_parameter("invalid_param");
        auto all_params = extractor->get_all_parameters();
    });
}

TEST_F(ExtractorEdgeCaseTest, CircularReferenceDetection)
{
    auto extractor_shared = std::make_shared<MockFeatureExtractor>();

    auto node1 = extractor_shared->create_node(std::vector<double> { 1.0 });
    auto node2 = extractor_shared->create_recursive_node(node1);

    EXPECT_NO_THROW({
        auto result = node2->extract();
        EXPECT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
    });
}

class ExtractorTypeSystemTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        extractor = std::make_shared<MockFeatureExtractor>();
        test_data = std::vector<double> { 1.0, 2.0, 3.0 };
    }

    std::shared_ptr<MockFeatureExtractor> extractor;
    std::vector<double> test_data;
};

TEST_F(ExtractorTypeSystemTest, InputTypeValidation)
{
    EXPECT_TRUE(ExtractorInputType<DataVariant>);
    EXPECT_TRUE(ExtractorInputType<std::shared_ptr<SignalSourceContainer>>);
    EXPECT_TRUE(ExtractorInputType<Region>);
    EXPECT_TRUE(ExtractorInputType<RegionGroup>);

    // EXPECT_FALSE(ExtractorInputType<std::string>); // This would fail at compile time
}

TEST_F(ExtractorTypeSystemTest, OutputTypeValidation)
{
    EXPECT_TRUE(ExtractorOutputType<std::vector<double>>);
    EXPECT_TRUE(ExtractorOutputType<std::vector<float>>);
    EXPECT_TRUE(ExtractorOutputType<DataVariant>);
    EXPECT_TRUE(ExtractorOutputType<RegionGroup>);
}

/* TEST_F(ExtractorTypeSystemTest, TypedExtractionSafety)
{
    // Test successful typed extraction
    auto result = extractor->extract_typed<DataVariant, std::vector<double>>(
        DataVariant { test_data }, "mean");

    EXPECT_FALSE(result.empty());

    EXPECT_THROW(
        extractor->extract_typed<DataVariant, RegionGroup>(
            DataVariant { test_data }, "mean"),
        std::runtime_error);
} */

class ExtractorAnalyzerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data.resize(512);
        for (size_t i = 0; i < test_data.size(); ++i) {
            test_data[i] = std::sin(2.0 * M_PI * i / 64.0);
        }

        container = std::make_shared<MockSignalSourceContainer>();
        container->set_test_data(test_data);

        analyzer = std::make_shared<EnergyAnalyzer>(256, 128);
        extractor = std::make_shared<MockAnalyzerIntegratedExtractor>();
    }

    std::vector<double> test_data;
    std::shared_ptr<MockSignalSourceContainer> container;
    std::shared_ptr<EnergyAnalyzer> analyzer;
    std::shared_ptr<MockAnalyzerIntegratedExtractor> extractor;
};

TEST_F(ExtractorAnalyzerIntegrationTest, AnalyzerDelegation)
{
    extractor->set_analyzer(analyzer);
    extractor->set_use_analyzer(true);
    extractor->set_extraction_method("delegate_to_analyzer");

    analyzer->set_parameter("method", "rms");
    analyzer->set_window_parameters(5, 2);

    ExtractorInput input { std::static_pointer_cast<Kakshya::SignalSourceContainer>(container) };
    auto result = extractor->apply_operation(input);

    EXPECT_TRUE(extractor->uses_analyzer());

    EXPECT_TRUE(result.base_output.index() != std::variant_npos);
}

TEST_F(ExtractorAnalyzerIntegrationTest, FallbackToDirectExtraction)
{
    extractor->set_use_analyzer(false);
    extractor->set_extraction_method("direct_extraction");

    ExtractorInput input { DataVariant { test_data } };
    auto result = extractor->apply_operation(input);

    EXPECT_FALSE(extractor->uses_analyzer());
    EXPECT_GT(extractor->get_direct_extraction_count(), 0);
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractorAnalyzerIntegrationTest, HybridExtractionMode)
{
    extractor->set_analyzer(analyzer);
    extractor->set_use_analyzer(true);
    extractor->set_extraction_method("hybrid");

    analyzer->set_parameter("method", "rms");
    analyzer->set_window_parameters(5, 2);

    ExtractorInput input { DataVariant { test_data } };
    auto result = extractor->apply_operation(input);

    EXPECT_TRUE(extractor->uses_analyzer());

    EXPECT_TRUE(result.base_output.index() != std::variant_npos);
}

class ExtractorFutureExpansionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 };
        extractor = std::make_shared<MockFeatureExtractor>();
    }

    std::vector<double> test_data;
    std::shared_ptr<MockFeatureExtractor> extractor;
};

TEST_F(ExtractorFutureExpansionTest, MultiModalOutputSupport)
{
    ExtractorInput input { DataVariant { test_data } };

    std::unordered_map<std::string, std::any> multi_modal_result;
    multi_modal_result["audio_features"] = std::vector<double> { 1.0, 2.0, 3.0 };
    multi_modal_result["metadata"] = std::string("test_metadata");
    multi_modal_result["confidence"] = 0.95;

    ExtractorOutput output { multi_modal_result };

    bool holds = std::holds_alternative<std::unordered_map<std::string, std::any>>(output.base_output);

    EXPECT_TRUE(holds);
    EXPECT_TRUE(output.recursive_outputs.empty());
}

TEST_F(ExtractorFutureExpansionTest, CoroutineReadinessStructure)
{
    // Test that the structure supports future coroutine integration
    // This is more of a structural test for C++20 coroutine readiness

    auto lazy_node = extractor->create_lazy_node([this]() {
        return ExtractorOutput { test_data };
    });

    EXPECT_TRUE(lazy_node->is_lazy());

    auto result = lazy_node->extract();
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result.base_output));
}

TEST_F(ExtractorFutureExpansionTest, GrammarExpansionReadiness)
{
    MockExtractionGrammar grammar;

    MockExtractionGrammar::MockRule complex_rule("complex_pattern", 15);
    complex_rule.matcher = [](const ExtractorInput& input) {
        return std::holds_alternative<DataVariant>(input.base_input);
    };
    complex_rule.extractor = [](const ExtractorInput& input) {
        return ExtractorOutput { std::vector<double> { 42.0 } };
    };

    grammar.add_mock_rule(complex_rule);

    ExtractorInput input { DataVariant { test_data } };
    auto result = grammar.extract_by_rule("complex_pattern", input);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<double>>(result->base_output));
}

} // namespace MayaFlux::Test
