#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Yantra/Analyzers/UniversalAnalyzer.hpp"
#include "MayaFlux/Yantra/ComputeMatrix.hpp"

namespace MayaFlux::Yantra {

/**
 * @brief Forward declaration for extractor node and universal extractor types.
 */
class ExtractorNode;
class UniversalExtractor;

/**
 * @brief Base extraction input variant - compatible with AnalyzerInput.
 *
 * Represents all supported input types for extractors and analyzers, including raw data,
 * signal containers, regions, region groups, region segments, and analyzer outputs.
 */
using BaseExtractorInput = std::variant<
    Kakshya::DataVariant, ///< Raw multi-type data (e.g., audio, numeric, etc.)
    std::shared_ptr<Kakshya::SignalSourceContainer>, ///< N-dimensional signal container
    Kakshya::Region, ///< Single region of interest
    Kakshya::RegionGroup, ///< Group of regions
    std::vector<Kakshya::RegionSegment>, ///< List of attributed segments
    AnalyzerOutput ///< Output from an analyzer, for chaining or feedback
    >;

/**
 * @struct ExtractorInput
 * @brief Complete extraction input with recursive support via type erasure.
 *
 * Encapsulates the input to an extractor, supporting both the main input (variant)
 * and a list of recursive inputs (ExtractorNode). This enables recursive and feedback
 * extraction patterns, as well as seamless integration with analyzers.
 */
struct ExtractorInput {
    BaseExtractorInput base_input; ///< Main input value (variant)
    std::vector<std::shared_ptr<ExtractorNode>> recursive_inputs; ///< Recursive/self-referential inputs

    // --- Constructors for seamless usage ---

    /**
     * @brief Construct from a base input variant.
     * @param input The base input variant.
     */
    ExtractorInput(const BaseExtractorInput& input)
        : base_input(input)
    {
    }

    /**
     * @brief Construct from a Kakshya::DataVariant.
     * @param data Raw data variant.
     */
    ExtractorInput(Kakshya::DataVariant data)
        : base_input(data)
    {
    }

    /**
     * @brief Construct from a SignalSourceContainer.
     * @param container Shared pointer to signal container.
     */
    ExtractorInput(std::shared_ptr<Kakshya::SignalSourceContainer> container)
        : base_input(container)
    {
    }

    /**
     * @brief Construct from a Region.
     * @param region Region of interest.
     */
    ExtractorInput(const Kakshya::Region& region)
        : base_input(region)
    {
    }

    /**
     * @brief Construct from a RegionGroup.
     * @param group Group of regions.
     */
    ExtractorInput(const Kakshya::RegionGroup& group)
        : base_input(group)
    {
    }

    /**
     * @brief Construct from a list of RegionSegments.
     * @param segments List of region segments.
     */
    ExtractorInput(const std::vector<Kakshya::RegionSegment>& segments)
        : base_input(segments)
    {
    }

    /**
     * @brief Construct from an AnalyzerOutput.
     * @param output Output from an analyzer.
     */
    ExtractorInput(const AnalyzerOutput& output)
        : base_input(output)
    {
    }

    /**
     * @brief Add a recursive input node for advanced extraction patterns.
     * @param node Shared pointer to an ExtractorNode.
     */
    void add_recursive_input(std::shared_ptr<ExtractorNode> node)
    {
        recursive_inputs.push_back(node);
    }

    /**
     * @brief Check if any recursive inputs are present.
     * @return True if recursive inputs exist, false otherwise.
     */
    bool has_recursive_inputs() const { return !recursive_inputs.empty(); }
};

/**
 * @brief Base extraction output variant.
 *
 * Represents all supported output types for extractors, including numeric sequences,
 * complex/spectral data, raw data, region groups, region segments, and multi-modal results.
 */
using BaseExtractorOutput = std::variant<
    std::vector<double>, ///< Simple numeric sequences (e.g., features)
    std::vector<float>, ///< Lower precision numeric sequences
    std::vector<std::complex<double>>, ///< Complex/spectral data
    Kakshya::DataVariant, ///< Raw data output (e.g., for chaining)
    Kakshya::RegionGroup, ///< Extracted regions
    std::vector<Kakshya::RegionSegment>, ///< Attributed segments
    std::unordered_map<std::string, std::any> ///< Multi-modal results (flexible, for advanced use)
    >;

/**
 * @struct ExtractorOutput
 * @brief Complete extraction output with recursive support.
 *
 * Encapsulates the output from an extractor, supporting both the main output (variant)
 * and a list of recursive outputs (ExtractorNode). This enables recursive/lazy extraction,
 * feedback, and multi-stage pipelines.
 */
struct ExtractorOutput {
    BaseExtractorOutput base_output; ///< Main output value (variant)
    std::vector<std::shared_ptr<ExtractorNode>> recursive_outputs; ///< Recursive/lazy outputs

    ExtractorOutput() = default;

    /**
     * @brief Construct from a base output variant.
     * @param output The base output variant.
     */
    ExtractorOutput(const BaseExtractorOutput& output)
        : base_output(output)
    {
    }

    /**
     * @brief Construct from a vector of doubles.
     * @param data Numeric feature vector.
     */
    ExtractorOutput(std::vector<double> data)
        : base_output(data)
    {
    }

    /**
     * @brief Construct from a vector of floats.
     * @param data Numeric feature vector (float).
     */
    ExtractorOutput(std::vector<float> data)
        : base_output(data)
    {
    }

    /**
     * @brief Construct from a vector of complex doubles.
     * @param data Complex/spectral feature vector.
     */
    ExtractorOutput(std::vector<std::complex<double>> data)
        : base_output(data)
    {
    }

    /**
     * @brief Construct from a Kakshya::DataVariant.
     * @param data Raw data output.
     */
    ExtractorOutput(Kakshya::DataVariant data)
        : base_output(data)
    {
    }

    /**
     * @brief Construct from a RegionGroup.
     * @param group Extracted region group.
     */
    ExtractorOutput(Kakshya::RegionGroup group)
        : base_output(group)
    {
    }

    /**
     * @brief Construct from a vector of RegionSegments.
     * @param segments Attributed region segments.
     */
    ExtractorOutput(std::vector<Kakshya::RegionSegment> segments)
        : base_output(segments)
    {
    }

    /**
     * @brief Construct from a multi-modal result map.
     * @param multi_modal Map of named results (string to any).
     */
    ExtractorOutput(std::unordered_map<std::string, std::any> multi_modal)
        : base_output(multi_modal)
    {
    }

    /**
     * @brief Add a recursive output node for advanced extraction patterns.
     * @param node Shared pointer to an ExtractorNode.
     */
    void add_recursive_output(std::shared_ptr<ExtractorNode> node)
    {
        recursive_outputs.push_back(node);
    }

    /**
     * @brief Check if any recursive outputs are present.
     * @return True if recursive outputs exist, false otherwise.
     */
    bool has_recursive_outputs() const { return !recursive_outputs.empty(); }
};

/**
 * @class ExtractorNode
 * @brief Type-erased node that can hold any extraction result.
 *
 * Abstract base class for nodes in the extraction tree. Each node can contain either a concrete
 * result or a reference to another extractor, enabling recursive, lazy, and composable extraction.
 * Used for advanced extraction patterns, feedback, and lazy evaluation.
 */
class ExtractorNode {
public:
    /**
     * @brief Virtual destructor for safe polymorphic use.
     */
    virtual ~ExtractorNode() = default;

    /**
     * @brief Extract the result from this node.
     * @return ExtractorOutput containing the result.
     */
    virtual ExtractorOutput extract() = 0;

    /**
     * @brief Get the type name of the result held by this node.
     * @return Type name as a string.
     */
    virtual std::string get_type_name() const = 0;

    /**
     * @brief Check if this node represents a lazy (deferred) computation.
     * @return True if lazy, false otherwise.
     */
    virtual bool is_lazy() const { return false; }

    /**
     * @brief Attempt to get the result as a specific type.
     * @tparam T Desired type.
     * @return Optional containing the result if type matches, std::nullopt otherwise.
     */
    template <typename T>
    std::optional<T> get_as() const
    {
        auto output = const_cast<ExtractorNode*>(this)->extract();
        if (auto* typed_result = std::get_if<T>(&output.base_output)) {
            return *typed_result;
        }
        return std::nullopt;
    }
};

}
