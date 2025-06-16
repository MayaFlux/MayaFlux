#pragma once

#include "MayaFlux/Yantra/Extractors/ExtractionHelper.hpp"
#include "MayaFlux/Yantra/Extractors/UniversalExtractor.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

/**
 * @class MockUniversalExtractor
 * @brief Mock implementation of UniversalExtractor for testing
 */
class MockUniversalExtractor : public UniversalExtractor {
public:
    MockUniversalExtractor()
        : m_mock_result(std::vector<double> { 42.0 })
        , m_extraction_count(0)
        , m_should_throw(false)
    {
    }

    // Mock configuration methods
    void set_mock_result(const ExtractorOutput& result)
    {
        m_mock_result = result;
    }

    void set_should_throw(bool should_throw)
    {
        m_should_throw = should_throw;
    }

    void clear_extraction_count()
    {
        m_extraction_count = 0;
    }

    size_t get_extraction_count() const
    {
        return m_extraction_count;
    }

    // Mock data access
    const ExtractorOutput& get_last_result() const
    {
        return m_mock_result;
    }

    std::vector<std::string> get_available_methods() const override
    {
        return { "mock_method", "test_method", "default" };
    }

protected:
    ExtractorOutput extract_impl(const DataVariant& data) override
    {
        m_extraction_count++;

        if (m_should_throw) {
            throw std::runtime_error("Mock extractor error");
        }

        // Simulate processing based on method
        const std::string method = get_extraction_method();

        if (method == "mock_method") {
            return ExtractorOutput { std::vector<double> { 1.0, 2.0, 3.0 } };
        } else if (method == "test_method") {
            return ExtractorOutput { std::vector<double> { 99.9 } };
        }

        return m_mock_result;
    }

    ExtractorOutput extract_impl(std::shared_ptr<SignalSourceContainer> container) override
    {
        m_extraction_count++;

        if (m_should_throw) {
            throw std::runtime_error("Mock extractor container error");
        }

        // Mock container processing
        return ExtractorOutput { std::vector<double> { static_cast<double>(container->get_total_elements()) } };
    }

    ExtractorOutput extract_impl(const Region& region) override
    {
        m_extraction_count++;

        if (m_should_throw) {
            throw std::runtime_error("Mock extractor region error");
        }

        return ExtractorOutput { std::vector<double> {
            static_cast<double>(region.start_coordinates[0]),
            static_cast<double>(region.start_coordinates[0]) } };
    }

    ExtractorOutput extract_impl(const RegionGroup& group) override
    {
        m_extraction_count++;

        if (m_should_throw) {
            throw std::runtime_error("Mock extractor region group error");
        }

        return ExtractorOutput { std::vector<double> {
            static_cast<double>(group.regions.size()) } };
    }

    ExtractorOutput extract_impl(const std::vector<RegionSegment>& segments) override
    {
        m_extraction_count++;

        if (m_should_throw) {
            throw std::runtime_error("Mock extractor segments error");
        }

        return ExtractorOutput { std::vector<double> {
            static_cast<double>(segments.size()) } };
    }

    std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const override
    {
        return get_available_methods();
    }

private:
    ExtractorOutput m_mock_result;
    mutable size_t m_extraction_count;
    bool m_should_throw;
};

/**
 * @class MockFeatureExtractor
 * @brief Mock feature extractor with specific feature simulation
 */
class MockFeatureExtractor : public UniversalExtractor {
public:
    MockFeatureExtractor()
        : m_simulate_realistic_features(true)
    {
    }

    void set_simulate_realistic_features(bool simulate)
    {
        m_simulate_realistic_features = simulate;
    }

    std::vector<std::string> get_available_methods() const override
    {
        return { "mean", "variance", "energy", "rms", "peak", "zero_crossings",
            "spectral_centroid", "spectral_rolloff", "mfcc", "chroma" };
    }

protected:
    ExtractorOutput extract_impl(const DataVariant& data) override
    {
        std::vector<double> audio_data = convert_variant_to_double(data);
        const std::string method = get_extraction_method();

        if (!m_simulate_realistic_features) {
            return ExtractorOutput { std::vector<double> { 42.0 } };
        }

        if (method == "mean") {
            double mean = std::accumulate(audio_data.begin(), audio_data.end(), 0.0) / audio_data.size();
            return ExtractorOutput { std::vector<double> { mean } };
        } else if (method == "variance") {
            double mean = std::accumulate(audio_data.begin(), audio_data.end(), 0.0) / audio_data.size();
            double variance = 0.0;
            for (double val : audio_data) {
                variance += (val - mean) * (val - mean);
            }
            variance /= audio_data.size();
            return ExtractorOutput { std::vector<double> { variance } };
        } else if (method == "energy") {
            double energy = 0.0;
            for (double val : audio_data) {
                energy += val * val;
            }
            return ExtractorOutput { std::vector<double> { energy } };
        } else if (method == "rms") {
            double rms = 0.0;
            for (double val : audio_data) {
                rms += val * val;
            }
            rms = std::sqrt(rms / audio_data.size());
            return ExtractorOutput { std::vector<double> { rms } };
        } else if (method == "peak") {
            auto max_it = std::max_element(audio_data.begin(), audio_data.end(),
                [](double a, double b) { return std::abs(a) < std::abs(b); });
            return ExtractorOutput { std::vector<double> { *max_it } };
        } else if (method == "zero_crossings") {
            size_t crossings = 0;
            for (size_t i = 1; i < audio_data.size(); ++i) {
                if ((audio_data[i - 1] >= 0) != (audio_data[i] >= 0)) {
                    crossings++;
                }
            }
            return ExtractorOutput { std::vector<double> { static_cast<double>(crossings) } };
        } else if (method == "spectral_centroid") {
            // Mock spectral centroid calculation
            return ExtractorOutput { std::vector<double> { 1000.0 + static_cast<double>(audio_data.size()) } };
        } else if (method == "spectral_rolloff") {
            // Mock spectral rolloff
            return ExtractorOutput { std::vector<double> { 3000.0 + static_cast<double>(audio_data.size()) } };
        } else if (method == "mfcc") {
            // Mock MFCC coefficients (typically 12-13 coefficients)
            std::vector<double> mfcc_coeffs;
            for (int i = 0; i < 13; ++i) {
                mfcc_coeffs.push_back(static_cast<double>(i) * 0.1 + std::sin(static_cast<double>(i)) * 0.05);
            }
            return ExtractorOutput { mfcc_coeffs };
        } else if (method == "chroma") {
            // Mock chroma features (12 semitones)
            std::vector<double> chroma_features;
            for (int i = 0; i < 12; ++i) {
                chroma_features.push_back(std::abs(std::sin(i * 0.5)) * 0.8 + 0.1);
            }
            return ExtractorOutput { chroma_features };
        }

        // Default: return input data
        return ExtractorOutput { audio_data };
    }

    ExtractorOutput extract_impl(std::shared_ptr<SignalSourceContainer> container) override
    {
        return ExtractorOutput { std::vector<double> { static_cast<double>(container->get_total_elements()) } };
    }

    ExtractorOutput extract_impl(const Region& region) override
    {
        return ExtractorOutput { std::vector<double> { static_cast<double>(region.start_coordinates[0]) } };
    }

    ExtractorOutput extract_impl(const RegionGroup& group) override
    {
        return ExtractorOutput { std::vector<double> { static_cast<double>(group.regions.size()) } };
    }

    std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const override
    {
        return get_available_methods();
    }

private:
    bool m_simulate_realistic_features;

    // Helper function to convert DataVariant to double vector
    std::vector<double> convert_variant_to_double(const DataVariant& data)
    {
        return std::visit([](auto&& arg) -> std::vector<double> {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::vector<double>>) {
                return arg;
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                std::vector<double> result;
                result.reserve(arg.size());
                std::transform(arg.begin(), arg.end(), std::back_inserter(result),
                    [](float f) { return static_cast<double>(f); });
                return result;
            } else {
                // For other types, return a default vector
                return std::vector<double> { 1.0, 2.0, 3.0, 4.0, 5.0 };
            }
        },
            data);
    }
};

/**
 * @class MockExtractorNode
 * @brief Mock node for testing node-based operations
 */
template <typename T>
class MockExtractorNode : public ExtractorNode {
public:
    MockExtractorNode(T result, bool is_lazy = false)
        : m_result(std::move(result))
        , m_is_lazy(is_lazy)
        , m_extraction_count(0)
    {
    }

    ExtractorOutput extract() override
    {
        m_extraction_count++;
        return ExtractorOutput { m_result };
    }

    std::string get_type_name() const override
    {
        return std::string("MockExtractorNode<") + typeid(T).name() + ">";
    }

    bool is_lazy() const override
    {
        return m_is_lazy;
    }

    size_t get_extraction_count() const
    {
        return m_extraction_count;
    }

    const T& get_mock_result() const
    {
        return m_result;
    }

private:
    T m_result;
    bool m_is_lazy;
    mutable size_t m_extraction_count;
};

/**
 * @class MockExtractorChain
 * @brief Mock chain for testing chaining logic
 */
class MockExtractorChain {
public:
    MockExtractorChain() = default;

    void add_mock_extractor(std::shared_ptr<MockUniversalExtractor> extractor,
        const std::string& name = "")
    {
        m_extractors.push_back({ extractor, name });
    }

    ExtractorOutput extract(ExtractorInput input)
    {
        if (m_extractors.empty()) {
            throw std::runtime_error("Empty mock chain");
        }

        ExtractorInput current_input = input;

        for (const auto& [extractor, name] : m_extractors) {
            auto result = extractor->apply_operation(current_input);
            // For simplicity, use the result as next input (would need proper conversion in real implementation)
            current_input = ExtractorInput { DataVariant { std::get<std::vector<double>>(result.base_output) } };
        }

        return m_extractors.back().first->apply_operation(current_input);
    }

    std::vector<std::string> get_extractor_names() const
    {
        std::vector<std::string> names;
        for (const auto& [extractor, name] : m_extractors) {
            names.push_back(name.empty() ? "unnamed_mock" : name);
        }
        return names;
    }

    size_t get_total_extraction_count() const
    {
        size_t total = 0;
        for (const auto& [extractor, name] : m_extractors) {
            total += extractor->get_extraction_count();
        }
        return total;
    }

private:
    std::vector<std::pair<std::shared_ptr<MockUniversalExtractor>, std::string>> m_extractors;
};

/**
 * @class MockExtractionGrammar
 * @brief Mock grammar for testing rule-based extraction
 */
class MockExtractionGrammar {
public:
    struct MockRule {
        std::string name;
        std::function<bool(const ExtractorInput&)> matcher;
        std::function<ExtractorOutput(const ExtractorInput&)> extractor;
        int priority;
        bool enabled;

        MockRule(const std::string& rule_name, int prio = 0)
            : name(rule_name)
            , priority(prio)
            , enabled(true)
        {
            // Default matcher - always matches DataVariant
            matcher = [](const ExtractorInput& input) {
                return std::holds_alternative<DataVariant>(input.base_input);
            };

            // Default extractor - returns rule name as feature
            extractor = [rule_name](const ExtractorInput& input) {
                return ExtractorOutput { std::vector<double> { static_cast<double>(rule_name.length()) } };
            };
        }
    };

    void add_mock_rule(const MockRule& rule)
    {
        m_rules.push_back(rule);
        std::sort(m_rules.begin(), m_rules.end(),
            [](const MockRule& a, const MockRule& b) { return a.priority > b.priority; });
    }

    std::optional<ExtractorOutput> extract_by_rule(const std::string& rule_name,
        const ExtractorInput& input)
    {
        auto it = std::find_if(m_rules.begin(), m_rules.end(),
            [&](const MockRule& r) { return r.name == rule_name && r.enabled; });

        if (it != m_rules.end() && it->matcher(input)) {
            return it->extractor(input);
        }

        return std::nullopt;
    }

    std::vector<ExtractorOutput> extract_all_matching(const ExtractorInput& input)
    {
        std::vector<ExtractorOutput> results;
        for (const auto& rule : m_rules) {
            if (rule.enabled && rule.matcher(input)) {
                results.push_back(rule.extractor(input));
            }
        }
        return results;
    }

    std::vector<std::string> get_available_rules() const
    {
        std::vector<std::string> names;
        for (const auto& rule : m_rules) {
            if (rule.enabled) {
                names.push_back(rule.name);
            }
        }
        return names;
    }

    void enable_rule(const std::string& rule_name, bool enabled = true)
    {
        auto it = std::find_if(m_rules.begin(), m_rules.end(),
            [&](MockRule& r) { return r.name == rule_name; });
        if (it != m_rules.end()) {
            it->enabled = enabled;
        }
    }

    size_t get_rule_count() const
    {
        return m_rules.size();
    }

private:
    std::vector<MockRule> m_rules;
};

/**
 * @class MockAnalyzerIntegratedExtractor
 * @brief Mock extractor that can work with analyzer delegation
 */
class MockAnalyzerIntegratedExtractor : public UniversalExtractor {
public:
    MockAnalyzerIntegratedExtractor()
        : m_delegation_count(0)
        , m_direct_extraction_count(0)
    {
        set_parameter("method", "rms");
    }

    std::vector<std::string> get_available_methods() const override
    {
        return { "delegate_to_analyzer", "direct_extraction", "hybrid" };
    }

    size_t get_delegation_count() const { return m_delegation_count; }
    size_t get_direct_extraction_count() const { return m_direct_extraction_count; }

    ExtractorOutput extract_via_analyzer_strategy(ExtractorInput input) override
    {
        m_delegation_count++;

        if (uses_analyzer()) {
            return UniversalExtractor::extract_via_analyzer_strategy(input);
        }

        return ExtractorOutput { std::vector<double> { 888.0 } };
    }

protected:
    ExtractorOutput extract_impl(const DataVariant& data) override
    {
        const std::string method = get_extraction_method();

        if (method == "delegate_to_analyzer" && uses_analyzer()) {
            m_delegation_count++;

            if (uses_analyzer()) {
                AnalyzerInput analyzer_input { data };
                return UniversalExtractor::extract_via_analyzer_strategy(ExtractorInput { data });
            }

            return ExtractorOutput { std::vector<double> { 999.0 } };
        } else if (method == "hybrid" && uses_analyzer()) {
            m_delegation_count++;

            if (uses_analyzer()) {
                return UniversalExtractor::extract_via_analyzer_strategy(ExtractorInput { data });
            }

            return ExtractorOutput { std::vector<double> { 555.0 } };
        } else {
            m_direct_extraction_count++;
            return ExtractorOutput { std::vector<double> { 123.0 } };
        }
    }

    ExtractorOutput extract_impl(std::shared_ptr<SignalSourceContainer> container) override
    {
        if (uses_analyzer()) {
            m_delegation_count++;
            return UniversalExtractor::extract_via_analyzer_strategy(ExtractorInput { container });
        }

        m_direct_extraction_count++;
        return ExtractorOutput { std::vector<double> { static_cast<double>(container->get_total_elements()) } };
    }

    std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const override
    {
        return get_available_methods();
    }

private:
    mutable size_t m_delegation_count;
    mutable size_t m_direct_extraction_count;
};

} // namespace MayaFlux::Test
