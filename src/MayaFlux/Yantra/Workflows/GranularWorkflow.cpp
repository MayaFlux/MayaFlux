#include "GranularWorkflow.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Region/Region.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Sorters/GpuSorter.hpp"

namespace MayaFlux::Yantra::Granular {

namespace {

    /* std::vector<double> extract_grain_samples(
        const Kakshya::Region& grain,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        uint32_t channel)
    {
        if (!container)
            return {};
        auto variants = container->get_region_data(grain);
        if (channel >= variants.size())
            return {};
        auto span = Kakshya::convert_variant<double>(variants[channel]);
        return { span.begin(), span.end() };
    } */

    /// Resolve a qualifier string to the canonical default for a given AnalysisType.
    std::string resolve_qualifier(AnalysisType type, const std::string& qualifier)
    {
        if (!qualifier.empty())
            return qualifier;

        switch (type) {
        case AnalysisType::FEATURE:
            return "rms";
        case AnalysisType::STATISTICAL:
            return "mean";
        default:
            error<std::invalid_argument>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "resolve_qualifier: no default qualifier for AnalysisType {}",
                static_cast<int>(type));
        }
    }

} // namespace

std::vector<double> extract_grain_samples(
    const Kakshya::Region& grain,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t channel)
{
    if (!container)
        return {};
    const auto& raw = container->get_data();
    if (channel >= raw.size())
        return {};

    auto full_span = Kakshya::convert_variant<double>(raw[channel]);

    if (grain.start_coordinates.empty() || grain.end_coordinates.empty())
        return {};

    const auto start = static_cast<size_t>(grain.start_coordinates[0]);
    const auto end = static_cast<size_t>(grain.end_coordinates[0]);

    if (start >= full_span.size())
        return {};

    const auto clamped_end = std::min(end + 1, full_span.size());
    return { full_span.begin() + start, full_span.begin() + clamped_end };
}

// ============================================================================
// SegmentOp
// ============================================================================

Datum<Kakshya::RegionGroup> SegmentOp::extract_implementation(
    const Datum<Kakshya::RegionGroup>& input)
{
    std::shared_ptr<Kakshya::SignalSourceContainer> container;

    if (input.container && *input.container)
        container = *input.container;
    else {
        auto param = this->get_parameter("container");
        if (param.has_value())
            container = safe_any_cast_or_default<std::shared_ptr<Kakshya::SignalSourceContainer>>(
                param, nullptr);
    }

    if (!container) {
        error<std::runtime_error>(
            Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "SegmentOp: no container available via Datum or parameters");
    }

    const auto grain_size = this->template get_parameter_or_default<uint32_t>("grain_size", 1024U);
    const auto hop_size = this->template get_parameter_or_default<uint32_t>("hop_size", 512U);
    const auto channel = this->template get_parameter_or_default<uint32_t>("channel", 0U);

    const auto& raw = container->get_data();

    if (channel >= raw.size()) {
        error<std::runtime_error>(
            Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "SegmentOp: channel {} out of range (container has {})",
            channel, raw.size());
    }

    const auto samples = Kakshya::convert_variant<double>(raw[channel]);
    const auto total = static_cast<uint64_t>(samples.size());

    Datum<Kakshya::RegionGroup> out { input.data, container };
    out.data.regions.clear();
    out.data.current_region_index = 0;
    out.data.active_indices.clear();

    for (uint64_t onset = 0; onset + grain_size <= total; onset += hop_size) {
        Kakshya::Region grain {
            std::vector { onset },
            std::vector { onset + grain_size - 1 }
        };
        grain.set_attribute("onset", static_cast<double>(onset));
        out.data.regions.push_back(std::move(grain));
    }

    out.data.set_attribute<uint32_t>("grain_size", grain_size);
    out.data.set_attribute<uint32_t>("hop_size", hop_size);
    out.data.set_attribute<uint32_t>("channel", channel);

    return out;
}

// ============================================================================
// AttributeOp — private helpers
// ============================================================================

std::shared_ptr<RegionGroupAnalyzer<Kakshya::RegionGroup>>
AttributeOp::make_default_analyzer(AnalysisType type)
{
    switch (type) {
    case AnalysisType::FEATURE:
        return std::make_shared<EnergyAnalyzer<Kakshya::RegionGroup, Kakshya::RegionGroup>>();
    case AnalysisType::STATISTICAL:
        return std::make_shared<StatisticalAnalyzer<Kakshya::RegionGroup, Kakshya::RegionGroup>>();
    default:
        error<std::invalid_argument>(
            Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "make_default_analyzer: AnalysisType {} has no registered default analyzer",
            static_cast<int>(type));
    }
}

double AttributeOp::extract_scalar(
    const std::any& analysis_result,
    AnalysisType type,
    const std::string& qualifier)
{
    const std::string q = resolve_qualifier(type, qualifier);

    if (type == AnalysisType::FEATURE) {
        const auto& ea = safe_any_cast_or_throw<EnergyAnalysis>(analysis_result);
        if (ea.channels.empty())
            return 0.0;
        const auto& ch = ea.channels[0];

        if (q == "rms" || q == "mean")
            return ch.mean_energy;
        if (q == "peak" || q == "max")
            return ch.max_energy;
        if (q == "min")
            return ch.min_energy;
        if (q == "variance")
            return ch.variance;
        if (q == "dynamic_range")
            return ch.max_energy - ch.min_energy;
        if (q == "zero_crossing")
            return ch.event_positions.empty() ? 0.0 : static_cast<double>(ch.event_positions.size());

        return ch.mean_energy;
    }

    if (type == AnalysisType::STATISTICAL) {
        const auto& sa = safe_any_cast_or_throw<StatisticalAnalysis>(analysis_result);
        if (sa.channel_statistics.empty())
            return 0.0;
        const auto& ch = sa.channel_statistics[0];

        if (q == "mean")
            return ch.mean_stat;
        if (q == "std_dev")
            return ch.stat_std_dev;
        if (q == "variance")
            return ch.stat_variance;
        if (q == "kurtosis")
            return ch.kurtosis;
        if (q == "skewness")
            return ch.skewness;
        if (q == "median")
            return ch.median;
        if (q == "max")
            return ch.max_stat;
        if (q == "min")
            return ch.min_stat;

        return ch.mean_stat;
    }

    return 0.0;
}

double AttributeOp::compute_grain_attribute(
    const Kakshya::Region& grain,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t channel,
    const ExecutionContext& ctx) const
{
    // Priority 1: span lambda
    if (ctx.execution_metadata.contains("attribute_executor")) {
        auto exec = safe_any_cast_or_default<AttributeExecutor>(
            ctx.execution_metadata.at("attribute_executor"), nullptr);
        if (exec) {
            const auto samples = extract_grain_samples(grain, container, channel);
            return exec(std::span<const double>(samples.data(), samples.size()), ctx);
        }
        // try {
        //     auto exec_direct = std::any_cast<AttributeExecutor>(
        //         ctx.execution_metadata.at("attribute_executor"));
        //     std::cout << "direct cast succeeded\n";
        //     const auto samples = extract_grain_samples(grain, container, channel);
        //     return exec_direct(std::span<const double>(samples.data(), samples.size()), ctx);
        // } catch (const std::bad_any_cast& e) {
        //     std::cout << "direct cast failed: " << e.what() << "\n";
        // }
    }

    auto param_exec = this->get_parameter("attribute_executor");
    if (param_exec.has_value()) {
        auto exec = safe_any_cast_or_default<AttributeExecutor>(param_exec, nullptr);
        if (exec) {
            const auto samples = extract_grain_samples(grain, container, channel);
            return exec(std::span<const double>(samples.data(), samples.size()), ctx);
        }
    }

    std::shared_ptr<void> raw_analyzer;
    if (ctx.execution_metadata.contains("analyzer"))
        raw_analyzer = safe_any_cast_or_default<std::shared_ptr<void>>(
            ctx.execution_metadata.at("analyzer"), nullptr);

    if (!raw_analyzer) {
        auto p = this->get_parameter("analyzer");
        if (p.has_value())
            raw_analyzer = safe_any_cast_or_default<std::shared_ptr<void>>(p, nullptr);
    }

    const std::string qualifier = [&]() -> std::string {
        if (ctx.execution_metadata.contains("analyzer_qualifier"))
            return safe_any_cast_or_default<std::string>(
                ctx.execution_metadata.at("analyzer_qualifier"), {});
        auto p = this->get_parameter("analyzer_qualifier");
        return p.has_value() ? safe_any_cast_or_default<std::string>(p, {}) : std::string {};
    }();

    if (raw_analyzer) {
        auto analyzer = std::static_pointer_cast<RegionGroupAnalyzer<Kakshya::RegionGroup>>(raw_analyzer);
        Datum<Kakshya::RegionGroup> grain_datum { Kakshya::RegionGroup {}, container };
        grain_datum.data.regions.push_back(grain);
        const auto result = analyzer->analyze_data(grain_datum);

        const AnalysisType type = [&]() {
            if (ctx.execution_metadata.contains("analysis_type"))
                return safe_any_cast_or_default<AnalysisType>(
                    ctx.execution_metadata.at("analysis_type"), AnalysisType::STATISTICAL);
            return analyzer->get_analysis_type();
        }();

        return extract_scalar(result, type, qualifier);
    }

    AnalysisType type = AnalysisType::STATISTICAL;
    if (ctx.execution_metadata.contains("analysis_type"))
        type = safe_any_cast_or_default<AnalysisType>(
            ctx.execution_metadata.at("analysis_type"), AnalysisType::STATISTICAL);
    else {
        auto p = this->get_parameter("analysis_type");
        if (p.has_value())
            type = safe_any_cast_or_default<AnalysisType>(p, AnalysisType::STATISTICAL);
    }

    auto analyzer = make_default_analyzer(type);
    Datum<Kakshya::RegionGroup> grain_datum { Kakshya::RegionGroup {}, container };
    grain_datum.data.regions.push_back(grain);
    const auto result = analyzer->analyze_data(grain_datum);
    return extract_scalar(result, type, qualifier);
}

// ============================================================================
// AttributeOp::analyze_implementation
// ============================================================================

Datum<Kakshya::RegionGroup> AttributeOp::analyze_implementation(
    const Datum<Kakshya::RegionGroup>& input)
{
    if (input.data.regions.empty())
        return input;

    std::shared_ptr<Kakshya::SignalSourceContainer> container;

    if (input.container && *input.container)
        container = *input.container;
    else {
        auto param = this->get_parameter("container");
        if (param.has_value())
            container = safe_any_cast_or_default<std::shared_ptr<Kakshya::SignalSourceContainer>>(
                param, nullptr);
    }

    if (!container) {
        error<std::runtime_error>(
            Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "AttributeOp: no container available via Datum or parameters");
    }

    const auto channel = this->template get_parameter_or_default<uint32_t>("channel", 0U);
    const auto feature_key = this->template get_parameter_or_default<std::string>("feature_key", std::string("feature"));

    ExecutionContext ctx;
    for (const auto& [k, v] : this->get_all_analysis_parameters())
        ctx.execution_metadata[k] = v;

    Datum<Kakshya::RegionGroup> out { input.data, container };

    for (auto& grain : out.data.regions) {
        const double value = compute_grain_attribute(grain, container, channel, ctx);
        grain.set_attribute(feature_key, value);
    }

    return out;
}

// ============================================================================
// SortOp
// ============================================================================

Datum<Kakshya::RegionGroup> SortOp::sort_implementation(
    const Datum<Kakshya::RegionGroup>& input)
{
    const auto feature_key = this->template get_parameter_or_default<std::string>("feature_key", std::string("feature"));
    const bool ascending = this->template get_parameter_or_default<bool>("ascending", true);
    const auto gpu_thresh = this->template get_parameter_or_default<uint32_t>("gpu_sort_threshold", 0U);
    const auto n = static_cast<uint32_t>(input.data.regions.size());

    Datum<Kakshya::RegionGroup> out = input;

    if (n == 0)
        return out;

    if (gpu_thresh > 0 && n >= gpu_thresh) {
        struct SortPC {
            uint32_t element_count;
            uint32_t pass_number;
            uint32_t ascending;
        };

        std::vector<float> values(n);
        for (uint32_t i = 0; i < n; ++i) {
            auto attr = out.data.regions[i].get_attribute<double>(feature_key);
            values[i] = attr ? static_cast<float>(*attr) : 0.0F;
        }

        std::vector<uint32_t> indices(n);
        std::iota(indices.begin(), indices.end(), 0U);

        auto executor = std::make_shared<ShaderExecutionContext<>>(
            GpuShaderConfig {
                .shader_path = "sort_by_attribute.comp",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(SortPC) });

        executor->in_out(0, values, GpuBufferBinding::ElementType::FLOAT32)
            .in_out(1, indices, GpuBufferBinding::ElementType::UINT32)
            .set_output_size(1, n * sizeof(uint32_t));

        const uint32_t n_passes = n * 2;
        executor->set_multipass(n_passes,
            [n, ascending](uint32_t pass, void* pc_data) {
                const SortPC pc {
                    .element_count = n,
                    .pass_number = pass & 1U,
                    .ascending = ascending ? 1U : 0U,
                };
                std::memcpy(pc_data, &pc, sizeof(SortPC));
            });

        auto gpu_sorter = std::make_shared<GpuSorter<>>(executor);

        Datum<std::vector<Kakshya::DataVariant>> sort_input {
            { Kakshya::DataVariant(std::vector<double>(values.begin(), values.end())) }
        };

        auto result = gpu_sorter->apply_operation(sort_input);
        const auto sorted_indices = ShaderExecutionContext<>::read_output<uint32_t>(result, 1);

        std::vector<Kakshya::Region> reordered(n);
        for (uint32_t i = 0; i < n; ++i)
            reordered[i] = out.data.regions[sorted_indices[i]];

        out.data.regions = std::move(reordered);
        out.data.current_region_index = 0;
        out.data.active_indices.clear();
        return out;
    }

    out.data.sort_by_attribute(feature_key);

    if (!ascending)
        std::ranges::reverse(out.data.regions);

    out.data.current_region_index = 0;
    out.data.active_indices.clear();

    return out;
}

// ============================================================================
// Grammar rule executors
// ============================================================================

const ComputationGrammar::Rule::Executor segment_grains =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    auto datum = safe_any_cast_or_throw<Datum<Kakshya::RegionGroup>>(input);
    auto op = std::make_shared<SegmentOp>();
    apply_context_parameters(op, ctx);
    return op->apply_operation(datum);
};

const ComputationGrammar::Rule::Executor attribute_grains =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    auto datum = safe_any_cast_or_throw<Datum<Kakshya::RegionGroup>>(input);
    auto op = std::make_shared<AttributeOp>();
    apply_context_parameters(op, ctx);
    return op->apply_operation(datum);
};

const ComputationGrammar::Rule::Executor sort_grains =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    auto datum = safe_any_cast_or_throw<Datum<Kakshya::RegionGroup>>(input);
    auto op = std::make_shared<SortOp>();
    apply_context_parameters(op, ctx);
    return op->apply_operation(datum);
};

// ============================================================================
// make_granular_matrix
// ============================================================================

std::shared_ptr<GranularMatrix> make_granular_matrix(
    ComputationContext attribution_context)
{
    auto matrix = std::make_shared<GranularMatrix>();

    matrix->create_operation<SegmentOp>("segment");
    matrix->create_operation<AttributeOp>("attribute");
    matrix->create_operation<SortOp>("sort");

    matrix->get_grammar()->create_rule("segment").with_context(ComputationContext::TEMPORAL).with_priority(100).with_description("Fixed hop-size grain segmentation").matches_type<Kakshya::RegionGroup>().executes(segment_grains).build();

    matrix->get_grammar()->create_rule("attribute").with_context(attribution_context).with_priority(75).with_description("Per-grain feature attribution via analyzer or lambda").matches_type<Kakshya::RegionGroup>().executes(attribute_grains).build();

    matrix->get_grammar()->create_rule("sort").with_context(ComputationContext::STRUCTURAL).with_priority(50).with_description("Attribute-keyed grain sort, CPU or GPU").matches_type<Kakshya::RegionGroup>().executes(sort_grains).build();

    return matrix;
}

// ============================================================================
// process — AnalysisType path
// ============================================================================

GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AnalysisType analysis_type,
    const std::string& qualifier,
    uint32_t channel,
    bool ascending,
    uint32_t gpu_sort_threshold)
{
    auto matrix = make_granular_matrix();
    auto ctx = make_granular_context(
        grain_size, hop_size, feature_key,
        analysis_type, qualifier,
        channel, ascending, gpu_sort_threshold);
    ctx.execution_metadata["container"] = container;

    return matrix->with(make_granular_input(container))
        .then<SegmentOp>("segment")
        .then<AttributeOp>("attribute")
        .then<SortOp>("sort")
        .to_io();
}

// ============================================================================
// process — span lambda path
// ============================================================================

GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AttributeExecutor executor,
    uint32_t channel,
    bool ascending,
    uint32_t gpu_sort_threshold)
{
    auto matrix = make_granular_matrix();
    auto ctx = make_granular_context(
        grain_size, hop_size, feature_key,
        std::move(executor),
        channel, ascending, gpu_sort_threshold);
    ctx.execution_metadata["container"] = container;

    return matrix->with(make_granular_input(container))
        .then<SegmentOp>("segment")
        .then<AttributeOp>("attribute")
        .then<SortOp>("sort")
        .to_io();
}

} // namespace MayaFlux::Yantra::Granular
