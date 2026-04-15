#include "GranularWorkflow.hpp"

#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Sorters/GpuSorter.hpp"

#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra::Granular {

namespace {

    std::string resolve_qualifier(AnalysisType type, const std::string& qualifier)
    {
        if (!qualifier.empty()) {
            if (type == AnalysisType::FEATURE && qualifier == "rms")
                return "mean_energy";
            if (type == AnalysisType::STATISTICAL && qualifier == "mean")
                return "mean_stat";
            return qualifier;
        }
        switch (type) {
        case AnalysisType::FEATURE:
            return "mean_energy";
        case AnalysisType::STATISTICAL:
            return "mean_stat";
        default:
            error<std::invalid_argument>(
                Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "resolve_qualifier: no default qualifier for AnalysisType {}",
                static_cast<int>(type));
        }
    }

    std::shared_ptr<Kakshya::SoundFileContainer> run_to_container(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        const ExecutionContext& ctx,
        ComputationContext attribution_context,
        GranularOutput output = GranularOutput::CONTAINER,
        GrainTaper taper = {})
    {
        auto matrix = make_granular_matrix(attribution_context, output, std::move(taper));
        auto seg_op = matrix->get_operation<SegmentOp>("segment");
        auto attr_op = matrix->get_operation<AttributeOp>("attribute");
        auto sort_op = matrix->get_operation<SortOp>("sort");
        apply_context_parameters(seg_op, ctx);
        apply_context_parameters(attr_op, ctx);
        apply_context_parameters(sort_op, ctx);

        auto input = make_granular_input(container);
        auto seg = seg_op->apply_operation(input);
        auto attr = attr_op->apply_operation(seg);
        auto sorted = sort_op->apply_operation(attr);

        std::any result_any;
        if (output == GranularOutput::CONTAINER_ADDITIVE) {
            ExecutionContext patched = ctx;
            if (taper) {
                patched.execution_metadata["grain_taper"] = taper;
            }
            result_any = reconstruct_grains_additive(std::any(sorted), patched);
        } else {
            result_any = reconstruct_grains(std::any(sorted), ctx);
        }

        auto result = safe_any_cast_or_throw<
            Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(result_any);

        return std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(result.data);
    }

} // namespace

const ComputationGrammar::Rule::Executor reconstruct_grains =
    [](const std::any& input, const ExecutionContext& /*ctx*/) -> std::any {
    auto datum = safe_any_cast_or_throw<Datum<Kakshya::RegionGroup>>(input);
    const auto& regions = datum.data.regions;

    if (regions.empty()) {
        error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "reconstruct_grains: RegionGroup contains no grains");
    }

    if (!datum.container || !*datum.container) {
        error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "reconstruct_grains: no source container in datum");
    }

    auto source = *datum.container;
    const auto num_ch = static_cast<uint32_t>(source->get_structure().get_channel_count());
    const auto org = source->get_structure().organization;
    const uint32_t sample_rate = [&]() -> uint32_t {
        if (auto sc = std::dynamic_pointer_cast<Kakshya::SoundStreamContainer>(source))
            return sc->get_sample_rate();
        return 48000U;
    }();

    const auto grain_sz = safe_any_cast_or_default<uint32_t>(
        datum.data.get_attribute<uint32_t>("grain_size").has_value()
            ? std::any(datum.data.get_attribute<uint32_t>("grain_size").value())
            : std::any {},
        1024U);

    std::vector<std::vector<double>> channel_data(num_ch);
    for (uint32_t ch = 0; ch < num_ch; ++ch)
        channel_data[ch].reserve(regions.size() * grain_sz);

    Kakshya::iterate_region_channels(regions, source, num_ch, {},
        [&channel_data](size_t /*gi*/, uint32_t ch, std::span<double> samples) {
            channel_data[ch].insert(channel_data[ch].end(), samples.begin(), samples.end());
        });

    return Datum<std::shared_ptr<Kakshya::SignalSourceContainer>> {
        std::dynamic_pointer_cast<Kakshya::SignalSourceContainer>(
            Kakshya::make_sound_file_container(std::move(channel_data), num_ch, sample_rate, org)),
        {}
    };
};

const ComputationGrammar::Rule::Executor reconstruct_grains_additive =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    auto datum = safe_any_cast_or_throw<Datum<Kakshya::RegionGroup>>(input);
    const auto& regions = datum.data.regions;

    if (regions.empty()) {
        error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "reconstruct_grains_additive: RegionGroup contains no grains");
    }

    if (!datum.container || !*datum.container) {
        error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "reconstruct_grains_additive: no source container in datum");
    }

    auto source = *datum.container;
    const auto num_ch = static_cast<uint32_t>(source->get_structure().get_channel_count());
    const auto org = source->get_structure().organization;
    const uint32_t sample_rate = [&]() -> uint32_t {
        if (auto sc = std::dynamic_pointer_cast<Kakshya::SoundStreamContainer>(source))
            return sc->get_sample_rate();
        return 48000U;
    }();

    const auto grain_sz = safe_any_cast_or_default<uint32_t>(
        datum.data.get_attribute<uint32_t>("grain_size").has_value()
            ? std::any(datum.data.get_attribute<uint32_t>("grain_size").value())
            : std::any {},
        1024U);
    const auto hop_sz = safe_any_cast_or_default<uint32_t>(
        datum.data.get_attribute<uint32_t>("hop_size").has_value()
            ? std::any(datum.data.get_attribute<uint32_t>("hop_size").value())
            : std::any {},
        grain_sz);

    const auto taper = safe_any_cast_or_default<GrainTaper>(
        ctx.execution_metadata.contains("grain_taper")
            ? ctx.execution_metadata.at("grain_taper")
            : std::any {},
        GrainTaper {});

    const size_t out_len = (regions.size() - 1) * hop_sz + grain_sz;
    std::vector<std::vector<double>> channel_data(num_ch, std::vector<double>(out_len, 0.0));

    Kakshya::iterate_region_channels(regions, source, num_ch, taper,
        [&channel_data, hop_sz](size_t gi, uint32_t ch, std::span<double> samples) {
            const size_t offset = gi * hop_sz;
            auto& dest = channel_data[ch];
            for (size_t s = 0; s < samples.size() && offset + s < dest.size(); ++s)
                dest[offset + s] += samples[s];
        });

    return Datum<std::shared_ptr<Kakshya::SignalSourceContainer>> {
        std::dynamic_pointer_cast<Kakshya::SignalSourceContainer>(
            Kakshya::make_sound_file_container(std::move(channel_data), num_ch, sample_rate, org)),
        {}
    };
};

// ============================================================================
// SegmentOp
// ============================================================================

Datum<Kakshya::RegionGroup> SegmentOp::extract_implementation(
    const Datum<Kakshya::RegionGroup>& input)
{
    std::shared_ptr<Kakshya::SignalSourceContainer> container;

    if (input.container && *input.container) {
        container = *input.container;
    } else {
        auto param = this->get_parameter("container");
        if (param.has_value()) {
            container = safe_any_cast_or_default<std::shared_ptr<Kakshya::SignalSourceContainer>>(
                param, nullptr);
        }
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
    const auto& structure = container->get_structure();
    const auto num_channels = structure.get_channel_count();
    const auto total = static_cast<uint64_t>(samples.size());

    Datum<Kakshya::RegionGroup> out { input.data, container };
    out.data.regions.clear();
    out.data.current_region_index = 0;
    out.data.active_indices.clear();

    for (uint64_t onset = 0; onset + grain_size <= total; onset += hop_size) {
        Kakshya::Region grain {
            std::vector<uint64_t> { onset, 0 },
            std::vector<uint64_t> { onset + grain_size - 1, num_channels - 1 }
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
    AttributeExecutor exec;

    if (ctx.execution_metadata.contains("attribute_executor")) {
        exec = safe_any_cast_or_default<AttributeExecutor>(
            ctx.execution_metadata.at("attribute_executor"), nullptr);
    }

    if (!exec) {
        if (auto p = this->get_parameter("attribute_executor"); p.has_value())
            exec = safe_any_cast_or_default<AttributeExecutor>(p, nullptr);
    }

    if (exec) {
        const auto samples = Kakshya::extract_region_channel(grain, container, channel);
        return exec(std::span<const double>(samples.data(), samples.size()), ctx);
    }

    const auto grain_variants = container->get_region_data(grain);

    const std::string qualifier = [&]() -> std::string {
        if (ctx.execution_metadata.contains("analyzer_qualifier")) {
            return safe_any_cast_or_default<std::string>(
                ctx.execution_metadata.at("analyzer_qualifier"), {});
        }
        auto p = this->get_parameter("analyzer_qualifier");
        return p.has_value() ? safe_any_cast_or_default<std::string>(p, {}) : std::string {};
    }();

    const AnalysisType type = [&]() {
        if (ctx.execution_metadata.contains("analysis_type")) {
            return safe_any_cast_or_default<AnalysisType>(
                ctx.execution_metadata.at("analysis_type"), AnalysisType::FEATURE);
        }

        auto p = this->get_parameter("analysis_type");
        return p.has_value()
            ? safe_any_cast_or_default<AnalysisType>(p, AnalysisType::FEATURE)
            : AnalysisType::FEATURE;
    }();

    std::shared_ptr<void> raw_analyzer;
    if (ctx.execution_metadata.contains("analyzer")) {
        raw_analyzer = safe_any_cast_or_default<std::shared_ptr<void>>(
            ctx.execution_metadata.at("analyzer"), nullptr);
    }

    if (!raw_analyzer) {
        if (auto p = this->get_parameter("analyzer"); p.has_value())
            raw_analyzer = safe_any_cast_or_default<std::shared_ptr<void>>(p, nullptr);
    }

    if (raw_analyzer) {
        if (type == AnalysisType::FEATURE) {
            return extract_scalar_energy(
                std::static_pointer_cast<VariantEnergyAnalyzer<>>(raw_analyzer)
                    ->analyze_energy(grain_variants),
                qualifier);
        }

        if (type == AnalysisType::STATISTICAL) {
            return extract_scalar_statistics(
                std::static_pointer_cast<VariantStatisticalAnalyzer<>>(raw_analyzer)
                    ->analyze_statistics(grain_variants),
                qualifier);
        }
        return 0.0;
    }

    if (type == AnalysisType::FEATURE) {
        return extract_scalar_energy(
            m_energy_analyzer.analyze_energy(grain_variants), qualifier);
    }

    if (type == AnalysisType::STATISTICAL) {
        return extract_scalar_statistics(
            m_stat_analyzer.analyze_statistics(grain_variants), qualifier);
    }

    return 0.0;
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

    if (input.container && *input.container) {
        container = *input.container;
    } else {
        auto param = this->get_parameter("container");
        if (param.has_value()) {
            container = safe_any_cast_or_default<std::shared_ptr<Kakshya::SignalSourceContainer>>(
                param, nullptr);
        }
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
    ComputationContext attribution_context,
    GranularOutput output,
    GrainTaper taper)
{
    auto matrix = std::make_shared<GranularMatrix>();

    matrix->create_operation<SegmentOp>("segment");
    matrix->create_operation<AttributeOp>("attribute");
    matrix->create_operation<SortOp>("sort");

    matrix->get_grammar()->create_rule("segment") //
        .with_context(ComputationContext::TEMPORAL)
        .with_priority(100)
        .with_description("Fixed hop-size grain segmentation")
        .matches_type<Kakshya::RegionGroup>()
        .executes(segment_grains)
        .build();

    matrix->get_grammar()->create_rule("attribute") //
        .with_context(attribution_context)
        .with_priority(75)
        .with_description("Per-grain feature attribution via analyzer or lambda")
        .matches_type<Kakshya::RegionGroup>()
        .executes(attribute_grains)
        .build();

    matrix->get_grammar()->create_rule("sort") //
        .with_context(ComputationContext::STRUCTURAL)
        .with_priority(50)
        .with_description("Attribute-keyed grain sort, CPU or GPU")
        .matches_type<Kakshya::RegionGroup>()
        .executes(sort_grains)
        .build();

    if (output == GranularOutput::CONTAINER) {
        matrix->get_grammar()->create_rule("reconstruct") //
            .with_context(ComputationContext::STRUCTURAL)
            .with_priority(25)
            .with_description("Stitch sorted grains into a SoundFileContainer")
            .matches_type<Kakshya::RegionGroup>()
            .executes(reconstruct_grains)
            .build();
    } else if (output == GranularOutput::CONTAINER_ADDITIVE) {
        auto ola_executor =
            [t = std::move(taper)](const std::any& in, const ExecutionContext& ctx) -> std::any {
            ExecutionContext patched = ctx;
            if (t) {
                patched.execution_metadata["grain_taper"] = t;
            }
            return reconstruct_grains_additive(in, patched);
        };

        matrix->get_grammar()->create_rule("reconstruct") //
            .with_context(ComputationContext::STRUCTURAL)
            .with_priority(25)
            .with_description("Additive grain reconstruction into SoundFileContainer")
            .matches_type<Kakshya::RegionGroup>()
            .executes(ola_executor)
            .build();
    }

    return matrix;
}

// ============================================================================
// process — AnalysisType path
// ============================================================================

GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config,
    const std::string& qualifier)
{
    auto matrix = make_granular_matrix(config.attribution_context);

    auto ctx = make_granular_context(config, analysis_type, qualifier);

    auto seg_op = matrix->get_operation<SegmentOp>("segment");
    auto attr_op = matrix->get_operation<AttributeOp>("attribute");
    auto sort_op = matrix->get_operation<SortOp>("sort");
    apply_context_parameters(seg_op, ctx);
    apply_context_parameters(attr_op, ctx);
    apply_context_parameters(sort_op, ctx);

    return matrix->with(make_granular_input(container))
        .template then<SegmentOp>("segment")
        .template then<AttributeOp>("attribute")
        .template then<SortOp>("sort")
        .to_io();
}

// ============================================================================
// process — AttributeExecutor path
// ============================================================================

GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    const GranularConfig& config)
{
    auto matrix = make_granular_matrix(config.attribution_context);

    auto ctx = make_granular_context(config, std::move(executor));

    auto seg_op = matrix->get_operation<SegmentOp>("segment");
    auto attr_op = matrix->get_operation<AttributeOp>("attribute");
    auto sort_op = matrix->get_operation<SortOp>("sort");
    apply_context_parameters(seg_op, ctx);
    apply_context_parameters(attr_op, ctx);
    apply_context_parameters(sort_op, ctx);

    return matrix->with(make_granular_input(container))
        .template then<SegmentOp>("segment")
        .template then<AttributeOp>("attribute")
        .template then<SortOp>("sort")
        .to_io();
}

// ============================================================================
// process_to_container — AnalysisType path
// ============================================================================

std::shared_ptr<Kakshya::SoundFileContainer> process_to_container(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config,
    const std::string& qualifier,
    GranularOutput output)
{
    auto ctx = make_granular_context(config, analysis_type, qualifier);
    ctx.execution_metadata["container"] = container;

    return run_to_container(container, ctx, config.attribution_context,
        output, config.taper);
}

// ============================================================================
// process_to_container — AttributeExecutor path
// ============================================================================

std::shared_ptr<Kakshya::SoundFileContainer> process_to_container(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    const GranularConfig& config,
    GranularOutput output)
{
    auto ctx = make_granular_context(config, std::move(executor));
    ctx.execution_metadata["container"] = container;

    return run_to_container(container, ctx, config.attribution_context,
        output, config.taper);
}

} // namespace MayaFlux::Yantra::Granular
