#include "UniversalExtractor.hpp"

namespace MayaFlux::Yantra {

/**
 * @class FeatureExtractor
 * @brief Extracts mathematical and statistical features from input data.
 *
 * FeatureExtractor provides a set of standard feature extraction methods for numeric and signal data,
 * such as mean, variance, energy, zero-crossings, and spectral centroid. It is designed to be used
 * as a building block in extraction pipelines or as a standalone extractor.
 *
 * ## Key Features
 * - **Multiple extraction methods:** Supports a variety of statistical and signal-based features.
 * - **Extensible:** Can be subclassed to add custom feature extraction logic.
 * - **Integration:** Compatible with UniversalExtractor pipelines and analyzer delegation.
 */
class FeatureExtractor : public UniversalExtractor {
public:
    /**
     * @brief Get the list of available extraction methods supported by this extractor.
     * @return Vector of method names (e.g., "mean", "variance", etc.).
     */
    inline std::vector<std::string> get_available_methods() const override
    {
        return { "mean", "variance", "energy", "zero_crossings", "spectral_centroid" };
    }

protected:
    /**
     * @brief Core extraction logic for Kakshya::DataVariant input.
     *        Dispatches to the appropriate feature extraction method based on configuration.
     * @param data Input data variant.
     * @return ExtractorOutput containing the extracted feature(s).
     */
    ExtractorOutput extract_impl(const Kakshya::DataVariant& data) override;

    /**
     * @brief Get supported extraction methods for a specific input type.
     * @param type_info Type index of the input.
     * @return Vector of method names.
     */
    inline std::vector<std::string> get_methods_for_type_impl(std::type_index) const override
    {
        return get_available_methods();
    }
};

/**
 * @brief Utility function to create a new extractor instance of the specified type.
 * @tparam ExtractorType The type of extractor to create.
 * @return Shared pointer to the created extractor.
 */
template <typename ExtractorType>
std::shared_ptr<ExtractorType> create_extractor()
{
    return std::make_shared<ExtractorType>();
}

/**
 * @brief Utility function to create a new extractor instance with an attached analyzer.
 * @tparam ExtractorType The type of extractor to create.
 * @param analyzer Shared pointer to the UniversalAnalyzer to use for delegation.
 * @return Shared pointer to the created extractor with analyzer configured.
 */
template <typename ExtractorType>
std::shared_ptr<ExtractorType> create_extractor_with_analyzer(std::shared_ptr<UniversalAnalyzer> analyzer)
{
    auto extractor = std::make_shared<ExtractorType>();
    extractor->set_analyzer(analyzer);
    extractor->set_use_analyzer(true);
    return extractor;
}

/**
 * @class ExtractionGrammar
 * @brief Provides grammar-based, rule-driven extraction for advanced workflows.
 *
 * ExtractionGrammar enables the definition and application of pattern-matching rules for extraction.
 * Each rule specifies a matcher, an extraction function, dependencies, and context. This allows for
 * flexible, composable, and context-aware extraction pipelines that can adapt to complex data structures.
 *
 * ## Key Features
 * - **Rule-based extraction:** Define custom rules for matching and extracting features.
 * - **Context-aware:** Supports multiple extraction contexts (temporal, spectral, spatial, etc.).
 * - **Dependencies:** Specify dependencies between rules for staged extraction.
 * - **Integration:** Can be used standalone or as part of a UniversalExtractor pipeline.
 */
class ExtractionGrammar {
public:
    /**
     * @enum ExtractionContext
     * @brief Contexts in which extraction rules can be applied.
     */
    enum class ExtractionContext {
        TEMPORAL, ///< Time-domain or sequential context.
        SPECTRAL, ///< Frequency-domain or spectral context.
        SPATIAL, ///< Spatial or geometric context.
        SEMANTIC, ///< Semantic or label-based context.
        STRUCTURAL ///< Structural or hierarchical context.
    };

    /**
     * @struct Rule
     * @brief Represents a single extraction rule.
     *
     * Each rule contains a name, a matcher function, an extraction function, a list of dependencies,
     * a context, and a priority for rule ordering.
     */
    struct Rule {
        std::string name; ///< Unique rule name.
        std::function<bool(const ExtractorInput&)> matcher; ///< Predicate to determine if rule applies.
        std::function<ExtractorOutput(const ExtractorInput&)> extractor; ///< Extraction logic.
        std::vector<std::string> dependencies; ///< Names of rules this rule depends on.
        ExtractionContext context; ///< Context in which this rule is valid.
        int priority = 0; ///< Rule priority for ordering (higher runs first).
    };

    /**
     * @brief Add a new extraction rule to the grammar.
     * @param rule The rule to add.
     */
    void add_rule(const Rule& rule);

    /**
     * @brief Attempt to extract using a specific rule by name.
     * @param rule_name Name of the rule to apply.
     * @param input Extraction input.
     * @return Optional ExtractorOutput if the rule matches and extraction succeeds.
     */
    std::optional<ExtractorOutput> extract_by_rule(const std::string& rule_name, const ExtractorInput& input) const;

    /**
     * @brief Extract using all rules that match the input.
     * @param input Extraction input.
     * @return Vector of ExtractorOutputs for all matching rules.
     */
    std::vector<ExtractorOutput> extract_all_matching(const ExtractorInput& input) const;

    /**
     * @brief Get the list of available rule names in this grammar.
     * @return Vector of rule names.
     */
    std::vector<std::string> get_available_rules() const;

private:
    std::vector<Rule> m_rules; ///< List of registered extraction rules.
};

/*
// TODO: This should be added to the UniversalExtractor class definition:
inline void add_grammar_support_to_universal_extractor()
{
private:
    ExtractionGrammar m_grammar;

public:
    void add_grammar_rule(const ExtractionGrammar::Rule& rule) {
        m_grammar.add_rule(rule);
    }

    ExtractorOutput extract_by_grammar(const ExtractorInput& input, const std::string& rule_name) {
        if (auto result = m_grammar.extract_by_rule(rule_name, input)) {
            return *result;
        }
        throw std::runtime_error("Grammar rule not found or doesn't match: " + rule_name);
    }

    std::vector<ExtractorOutput> extract_all_patterns(const ExtractorInput& input) {
        return m_grammar.extract_all_matching(input);
    }
}
*/

}
