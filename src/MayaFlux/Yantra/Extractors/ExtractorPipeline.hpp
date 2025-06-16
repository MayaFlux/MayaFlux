#include "Extractors.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ExtractorChain
 * @brief Chains multiple UniversalExtractor instances in sequence for staged feature extraction.
 *
 * ExtractorChain enables the construction of modular, multi-stage extraction pipelines by chaining
 * together multiple UniversalExtractor instances. Each extractor in the chain processes the output
 * of the previous stage, allowing for complex, composable feature extraction workflows.
 *
 * ## Key Features
 * - **Sequential execution:** Each extractor receives the output of the previous extractor as input.
 * - **Named stages:** Each extractor can be optionally named for pipeline introspection.
 * - **Automatic input/output conversion:** Handles conversion between ExtractorOutput and ExtractorInput.
 * - **Integration with ExtractionPipeline:** Used internally for high-level pipeline orchestration.
 */
class ExtractorChain {
public:
    /**
     * @brief Add a UniversalExtractor to the chain.
     * @param extractor Shared pointer to the extractor to add.
     * @param name Optional name for the extractor stage.
     */
    inline void add_extractor(std::shared_ptr<UniversalExtractor> extractor, const std::string& name = "")
    {
        m_extractors.push_back({ extractor, name });
    }

    /**
     * @brief Execute the extraction chain on the provided input.
     *        Each extractor processes the output of the previous stage.
     * @param input Initial input to the chain.
     * @return Final ExtractorOutput from the last extractor in the chain.
     * @throws std::runtime_error if the chain is empty.
     */
    ExtractorOutput extract(ExtractorInput input) const;

    /**
     * @brief Get the names of all extractors in the chain.
     * @return Vector of extractor stage names (or "unnamed" if not specified).
     */
    std::vector<std::string> get_extractor_names() const;

private:
    /**
     * @brief List of extractors and their associated names.
     */
    std::vector<std::pair<std::shared_ptr<UniversalExtractor>, std::string>> m_extractors;

    /**
     * @brief Convert an ExtractorOutput to an ExtractorInput for the next stage.
     *        Handles recursive outputs and type conversions.
     * @param output Output from the previous extractor.
     * @return ExtractorInput suitable for the next extractor in the chain.
     */
    ExtractorInput convert_output_to_input(const ExtractorOutput& output) const;
};

/**
 * @class ExtractionPipeline
 * @brief High-level pipeline for composable, rule-driven extraction workflows.
 *
 * ExtractionPipeline provides a user-friendly interface for building, configuring, and executing
 * complex extraction pipelines. It supports chaining multiple extractors, integrating grammar-based
 * extraction rules, and introspecting pipeline stages.
 *
 * ## Key Features
 * - **Composable:** Add extractors of any type, in any order.
 * - **Grammar integration:** Supports rule-based extraction for advanced workflows.
 * - **Flexible input:** Accepts various input types (data, containers, etc.).
 * - **Pipeline introspection:** Query the names and order of pipeline stages.
 * - **Custom extractors:** Add user-defined or preconfigured extractors.
 */
class ExtractionPipeline {
public:
    /**
     * @brief Construct an empty ExtractionPipeline.
     */
    ExtractionPipeline() = default;

    /**
     * @brief Add a new extractor of the specified type to the pipeline.
     * @tparam ExtractorType Type of the extractor (must derive from UniversalExtractor).
     * @param name Optional name for the extractor stage.
     * @return Reference to this pipeline (for chaining).
     */
    template <typename ExtractorType>
    ExtractionPipeline& add_extractor(const std::string& name = "")
    {
        auto extractor = std::make_shared<ExtractorType>();
        m_chain.add_extractor(extractor, name);
        return *this;
    }

    /**
     * @brief Add a custom UniversalExtractor instance to the pipeline.
     * @param extractor Shared pointer to the custom extractor.
     * @param name Optional name for the extractor stage.
     * @return Reference to this pipeline (for chaining).
     */
    inline ExtractionPipeline& add_custom_extractor(std::shared_ptr<UniversalExtractor> extractor, const std::string& name = "")
    {
        m_chain.add_extractor(extractor, name);
        return *this;
    }

    /**
     * @brief Add a grammar rule for rule-based extraction.
     *        Grammar rules are applied before the extractor chain.
     * @param rule ExtractionGrammar rule to add.
     * @return Reference to this pipeline (for chaining).
     */
    inline ExtractionPipeline& add_grammar_rule(const ExtractionGrammar::Rule& rule)
    {
        m_grammar_rules.push_back(rule);
        return *this;
    }

    /**
     * @brief Execute the pipeline on the provided input.
     *        Applies grammar rules (if any), then runs the extractor chain.
     * @param input Input to the pipeline (may include recursive nodes).
     * @return Final ExtractorOutput from the pipeline.
     */
    ExtractorOutput process(ExtractorInput input);

    /**
     * @brief Execute the pipeline on a Kakshya::DataVariant input.
     * @param data Input data variant.
     * @return Final ExtractorOutput from the pipeline.
     */
    inline ExtractorOutput process(Kakshya::DataVariant data)
    {
        return process(ExtractorInput { data });
    }

    /**
     * @brief Execute the pipeline on a SignalSourceContainer input.
     * @param container Shared pointer to signal container.
     * @return Final ExtractorOutput from the pipeline.
     */
    inline ExtractorOutput process(std::shared_ptr<Kakshya::SignalSourceContainer> container)
    {
        return process(ExtractorInput { container });
    }

    /**
     * @brief Get the names of all pipeline stages, including grammar rules if present.
     * @return Vector of stage names.
     */
    std::vector<std::string> get_pipeline_stages() const;

private:
    /**
     * @brief The underlying extractor chain for sequential extraction.
     */
    ExtractorChain m_chain;

    /**
     * @brief List of grammar rules for rule-based extraction.
     */
    std::vector<ExtractionGrammar::Rule> m_grammar_rules;

    /**
     * @brief Convert the first grammar extraction result to an ExtractorInput.
     *        Used internally when grammar rules are applied.
     * @param output Output from grammar extraction.
     * @return ExtractorInput for further processing in the pipeline.
     */
    ExtractorInput convert_first_result_to_input(const ExtractorOutput& output);
};

}
