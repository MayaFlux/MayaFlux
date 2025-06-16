#include "ExtractorPipeline.hpp"

namespace MayaFlux::Yantra {

ExtractorOutput ExtractorChain::extract(ExtractorInput input) const
{
    ExtractorInput current_input = input;

    for (const auto& [extractor, name] : m_extractors) {
        auto current_output = extractor->apply_operation(current_input);

        current_input = convert_output_to_input(current_output);
    }

    if (!m_extractors.empty()) {
        return m_extractors.back().first->apply_operation(current_input);
    }

    throw std::runtime_error("Empty extractor chain");
}

std::vector<std::string> ExtractorChain::get_extractor_names() const
{
    std::vector<std::string> names;
    for (const auto& [extractor, name] : m_extractors) {
        names.push_back(name.empty() ? "unnamed" : name);
    }
    return names;
}

ExtractorInput ExtractorChain::convert_output_to_input(const ExtractorOutput& output) const
{
    /* return std::visit([&output](auto&& arg) -> ExtractorInput {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            ExtractorInput result { arg };
            for (auto& recursive_output : output.recursive_outputs) {
                result.add_recursive_input(recursive_output);
            }
            return result;
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            ExtractorInput result { Kakshya::DataVariant { arg } };
            for (auto& recursive_output : output.recursive_outputs) {
                result.add_recursive_input(recursive_output);
            }
            return result;
        } else if constexpr (std::is_same_v<T, std::vector<float>>) {
            ExtractorInput result { Kakshya::DataVariant { arg } };
            for (auto& recursive_output : output.recursive_outputs) {
                result.add_recursive_input(recursive_output);
            }
            return result;
        } else if constexpr (std::is_same_v<T, std::vector<std::complex<double>>>) {
            ExtractorInput result { Kakshya::DataVariant { arg } };
            for (auto& recursive_output : output.recursive_outputs) {
                result.add_recursive_input(recursive_output);
            }
            return result;
        } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
            ExtractorInput result { arg };
            for (auto& recursive_output : output.recursive_outputs) {
                result.add_recursive_input(recursive_output);
            }
            return result;
        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
            ExtractorInput result { arg };
            for (auto& recursive_output : output.recursive_outputs) {
                result.add_recursive_input(recursive_output);
            }
            return result;
        } else {
            throw std::runtime_error("Cannot convert output type to input type in chain");
        }
    },
        output.base_output); */

    return std::visit(
        [](auto&& value) -> ExtractorInput {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
                return ExtractorInput { value };
            } else if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
                return ExtractorInput { value };
            } else if constexpr (std::is_same_v<T, Kakshya::Region>) {
                return ExtractorInput { value };
            } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
                return ExtractorInput { value };
            } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
                return ExtractorInput { value };
            } else if constexpr (std::is_same_v<T, AnalyzerOutput>) {
                return ExtractorInput { value };
            }

            else if constexpr (std::is_same_v<T, std::vector<double>>) {
                return ExtractorInput { Kakshya::DataVariant { value } };
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                return ExtractorInput { Kakshya::DataVariant { value } };
            } else if constexpr (std::is_same_v<T, std::vector<std::complex<double>>>) {
                return ExtractorInput { Kakshya::DataVariant { value } };
            } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, std::any>>) {
                auto it = value.find("data");
                if (it != value.end()) {
                    try {
                        if (auto* vec_double = std::any_cast<std::vector<double>>(&it->second)) {
                            return ExtractorInput { Kakshya::DataVariant { *vec_double } };
                        } else if (auto* vec_float = std::any_cast<std::vector<float>>(&it->second)) {
                            return ExtractorInput { Kakshya::DataVariant { *vec_float } };
                        } else if (auto* data_variant = std::any_cast<Kakshya::DataVariant>(&it->second)) {
                            return ExtractorInput { *data_variant };
                        }
                    } catch (const std::bad_any_cast&) {
                    }
                }

                for (const auto& field_name : { "audio_features", "features", "values", "result" }) {
                    auto it = value.find(field_name);
                    if (it != value.end()) {
                        try {
                            if (auto* vec_double = std::any_cast<std::vector<double>>(&it->second)) {
                                return ExtractorInput { Kakshya::DataVariant { *vec_double } };
                            }
                        } catch (const std::bad_any_cast&) {
                            continue;
                        }
                    }
                }

                return ExtractorInput { Kakshya::DataVariant { std::vector<double> {} } };
            } else {
                throw std::runtime_error("Cannot convert ExtractorOutput to ExtractorInput: unknown type");
            }
        },
        output.base_output);
}

ExtractorOutput ExtractionPipeline::process(ExtractorInput input)
{
    if (!m_grammar_rules.empty()) {
        ExtractionGrammar grammar;
        for (const auto& rule : m_grammar_rules) {
            grammar.add_rule(rule);
        }

        auto grammar_results = grammar.extract_all_matching(input);
        if (!grammar_results.empty()) {
            input = convert_first_result_to_input(grammar_results[0]);
        }
    }

    return m_chain.extract(input);
}

std::vector<std::string> ExtractionPipeline::get_pipeline_stages() const
{
    auto names = m_chain.get_extractor_names();
    if (!m_grammar_rules.empty()) {
        names.insert(names.begin(), "grammar_rules");
    }
    return names;
}

ExtractorInput ExtractionPipeline::convert_first_result_to_input(const ExtractorOutput& output)
{
    return std::visit([&output](auto&& arg) -> ExtractorInput {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            ExtractorInput result { arg };
            for (auto& recursive_output : output.recursive_outputs) {
                result.add_recursive_input(recursive_output);
            }
            return result;
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            return ExtractorInput { Kakshya::DataVariant { arg } };
        } else {
            throw std::runtime_error("Cannot convert grammar result to pipeline input");
        }
    },
        output.base_output);
}
}
