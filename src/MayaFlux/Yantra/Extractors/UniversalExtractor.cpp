#include "UniversalExtractor.hpp"
#include "MayaFlux/Yantra/YantraUtils.hpp"

namespace MayaFlux::Yantra {

ExtractorOutput UniversalExtractor::apply_operation(ExtractorInput input)
{
    if (input.has_recursive_inputs()) {
        return extract_with_recursive_inputs(input);
    }

    return std::visit([this](auto&& arg) -> ExtractorOutput {
        using T = std::decay_t<decltype(arg)>;

        if (should_use_analyzer()) {
            return extract_via_analyzer(arg);
        }

        return this->extract_impl(arg);
    },
        input.base_input);
}

ExtractorOutput UniversalExtractor::extract_with_strategy(ExtractorInput input, ExtractionStrategy strategy)
{
    switch (strategy) {
    case ExtractionStrategy::LAZY:
        return create_lazy_extraction(input);
    case ExtractionStrategy::RECURSIVE:
        return extract_recursive(input);
    case ExtractionStrategy::ANALYZER_DELEGATE:
        return extract_via_analyzer_strategy(input);
    default:
        return apply_operation(input);
    }
}

std::string UniversalExtractor::get_extraction_method() const
{
    auto param = get_parameter("method");
    if (param.has_value()) {
        try {
            return std::any_cast<std::string>(param);
        } catch (const std::bad_any_cast&) {
            return "default";
        }
    }
    return "default";
}

ExtractorOutput UniversalExtractor::create_lazy_extraction(ExtractorInput input)
{
    auto lazy_node = create_lazy_node([this, input]() {
        return apply_operation(input);
    });

    ExtractorOutput result { std::vector<double> {} }; // Dummy base output
    result.add_recursive_output(lazy_node);
    return result;
}

ExtractorOutput UniversalExtractor::extract_recursive(ExtractorInput input)
{
    auto first_result = apply_operation(input);

    auto recursive_node = create_recursive_node(create_node(first_result));

    ExtractorOutput result = first_result;
    result.add_recursive_output(recursive_node);
    return result;
}

ExtractorOutput UniversalExtractor::extract_with_recursive_inputs(ExtractorInput input)
{
    std::vector<ExtractorOutput> recursive_results;
    for (auto& recursive_input : input.recursive_inputs) {
        recursive_results.push_back(recursive_input->extract());
    }

    auto base_result = std::visit([this](auto&& arg) -> ExtractorOutput {
        return this->extract_impl(arg);
    },
        input.base_input);

    return combine_results(base_result, recursive_results);
}

ExtractorOutput UniversalExtractor::extract_via_analyzer_strategy(ExtractorInput input)
{
    return std::visit([this](auto&& arg) -> ExtractorOutput {
        return extract_via_analyzer(arg);
    },
        input.base_input);
}

ExtractorOutput UniversalExtractor::convert_from_analyzer_output(const AnalyzerOutput& output)
{
    return std::visit([](auto&& result) -> ExtractorOutput {
        using T = std::decay_t<decltype(result)>;

        if constexpr (std::is_same_v<T, std::vector<double>>) {
            return ExtractorOutput { result };
        } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
            return ExtractorOutput { result };
        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
            return ExtractorOutput { result };
        } else if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            return ExtractorOutput { result };
        } else {
            throw std::runtime_error("Cannot convert AnalyzerOutput type");
        }
    },
        output);
}

ExtractorOutput RecursiveExtractorNode::extract()
{
    auto input_result = m_input_node->extract();

    ExtractorInput recursive_input = std::visit(
        [](auto&& value) -> ExtractorInput {
            using T = std::decay_t<decltype(value)>;
            if constexpr (
                std::is_same_v<T, Kakshya::DataVariant>
                || std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>
                || std::is_same_v<T, Kakshya::Region>
                || std::is_same_v<T, Kakshya::RegionGroup>
                || std::is_same_v<T, std::vector<Kakshya::RegionSegment>>
                || std::is_same_v<T, AnalyzerOutput>) {
                return ExtractorInput { value };
            } else if constexpr (
                std::is_same_v<T, std::vector<double>>
                || std::is_same_v<T, std::vector<float>>
                || std::is_same_v<T, std::vector<std::complex<double>>>) {
                return ExtractorInput { Kakshya::DataVariant { value } };
            } else {
                throw std::runtime_error("Cannot convert ExtractorOutput to ExtractorInput: type not supported");
            }
        },
        input_result.base_output);

    // Add any recursive outputs from the input result as recursive inputs
    for (auto& recursive_output : input_result.recursive_outputs) {
        recursive_input.add_recursive_input(recursive_output);
    }

    return m_extraction_func(recursive_input);
}
}
