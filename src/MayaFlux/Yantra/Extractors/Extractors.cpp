#include "Extractors.hpp"
#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#include "oneapi/dpl/numeric"
#else
#include "execution"
#endif

namespace MayaFlux::Yantra {

ExtractorOutput FeatureExtractor::extract_impl(const Kakshya::DataVariant& data)
{
    const std::string method = get_extraction_method();

    std::vector<double> audio_data = Kakshya::convert_variant_to_double(data);

    if (method == "mean") {
        double mean = std::accumulate(audio_data.begin(), audio_data.end(), 0.0) / audio_data.size();
        return ExtractorOutput { std::vector<double> { mean } };
    } else if (method == "energy") {
        double energy = std::transform_reduce(std::execution::par_unseq,
            audio_data.begin(), audio_data.end(), 0.0, std::plus<>(),
            [](double x) { return x * x; });
        if (!audio_data.empty())
            energy /= audio_data.size();
        return ExtractorOutput { std::vector<double> { energy } };
    }

    return ExtractorOutput { audio_data }; // Default: return input
}

void ExtractionGrammar::add_rule(const Rule& rule)
{
    m_rules.push_back(rule);
    std::sort(m_rules.begin(), m_rules.end(),
        [](const Rule& a, const Rule& b) { return a.priority > b.priority; });
}

std::optional<ExtractorOutput> ExtractionGrammar::extract_by_rule(const std::string& rule_name, const ExtractorInput& input) const
{
    auto it = std::find_if(m_rules.begin(), m_rules.end(),
        [&](const Rule& r) { return r.name == rule_name; });
    if (it != m_rules.end() && it->matcher(input)) {
        return it->extractor(input);
    }
    return std::nullopt;
}

std::vector<ExtractorOutput> ExtractionGrammar::extract_all_matching(const ExtractorInput& input) const
{
    std::vector<ExtractorOutput> results;
    for (const auto& rule : m_rules) {
        if (rule.matcher(input)) {
            results.push_back(rule.extractor(input));
        }
    }
    return results;
}

std::vector<std::string> ExtractionGrammar::get_available_rules() const
{
    std::vector<std::string> names;
    std::transform(m_rules.begin(), m_rules.end(), std::back_inserter(names),
        [](const Rule& r) { return r.name; });
    return names;
}
}
