#include "GranularWorkflow.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra::Granular {

namespace {

    template <typename Sink>
    std::any reconstruct_with(std::string_view name, const std::any& input, const ExecutionContext& ctx,
        bool additive, Sink&& sink)
    {
        auto datum = safe_any_cast_or_throw<Datum<Kakshya::RegionGroup>>(input);
        const auto& regions = datum.data.regions;

        if (regions.empty()) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "{}: RegionGroup contains no grains", name);
        }

        if (!datum.container || !*datum.container) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "{}: no source container in datum", name);
        }

        auto source = *datum.container;
        const auto num_ch = static_cast<uint32_t>(source->get_structure().get_channel_count());
        const auto org = source->get_structure().organization;
        const uint32_t sample_rate = [&]() -> uint32_t {
            if (auto sc = std::dynamic_pointer_cast<Kakshya::SoundStreamContainer>(source))
                return sc->get_sample_rate();
            return 48000U;
        }();

        const auto grain_sz = (datum.data.get(grain_size_setting) ? *datum.data.get(grain_size_setting) : 1024U);
        const auto hop_sz = (datum.data.get(hop_size_setting) ? *datum.data.get(hop_size_setting) : grain_sz);

        std::vector<std::vector<double>> channel_data(num_ch);
        GrainTaper taper;

        if (additive) {
            const auto* supplied = ctx.facets.get(taper_facet);
            if (supplied)
                taper = *supplied;
            const size_t out_len = (regions.size() - 1) * hop_sz + grain_sz;
            for (auto& channel : channel_data)
                channel.assign(out_len, 0.0);
        } else {
            for (auto& channel : channel_data)
                channel.reserve(regions.size() * grain_sz);
        }

        Kakshya::iterate_region_channels(regions, source, num_ch, taper,
            [&channel_data, hop_sz, additive](size_t gi, uint32_t ch, std::span<double> samples) {
                auto& dest = channel_data[ch];
                if (!additive) {
                    dest.insert(dest.end(), samples.begin(), samples.end());
                    return;
                }
                const size_t offset = gi * hop_sz;
                for (size_t i = 0; i < samples.size() && offset + i < dest.size(); ++i)
                    dest[offset + i] += samples[i];
            });

        return Datum<std::shared_ptr<Kakshya::SignalSourceContainer>> {
            sink(std::move(channel_data), num_ch, sample_rate, org), {}
        };
    }

    constexpr auto to_container = [](std::vector<std::vector<double>>&& channels, uint32_t count,
                                      uint32_t sample_rate, const auto& organization)
        -> std::shared_ptr<Kakshya::SignalSourceContainer> {
        return std::dynamic_pointer_cast<Kakshya::SignalSourceContainer>(
            Kakshya::make_sound_file_container(std::move(channels), count, sample_rate, organization));
    };

    constexpr auto to_stream = [](std::vector<std::vector<double>>&& channels, uint32_t count,
                                   uint32_t sample_rate, const auto& /*organization*/)
        -> std::shared_ptr<Kakshya::SignalSourceContainer> {
        auto stream = std::make_shared<Kakshya::DynamicSoundStream>(sample_rate, count);

        std::vector<std::span<const double>> spans;
        spans.reserve(count);
        for (auto& channel : channels)
            spans.emplace_back(channel);

        stream->write_frames(spans, 0);
        return std::dynamic_pointer_cast<Kakshya::SignalSourceContainer>(stream);
    };

    std::shared_ptr<Kakshya::SoundFileContainer> run_to_container(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        const ExecutionContext& ctx,
        GranularOutput output = GranularOutput::CONTAINER)
    {
        auto matrix = make_granular_matrix(output);
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

        const auto result_any = output == GranularOutput::CONTAINER_ADDITIVE
            ? reconstruct_grains_additive(std::any(sorted), ctx)
            : reconstruct_grains(std::any(sorted), ctx);

        auto result = safe_any_cast_or_throw<
            Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(result_any);

        return std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(result.data);
    }

    std::shared_ptr<Kakshya::DynamicSoundStream> run_to_stream(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        const ExecutionContext& ctx,
        GranularOutput output = GranularOutput::STREAM)
    {
        auto matrix = make_granular_matrix(output);
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

        const auto result_any = output == GranularOutput::STREAM_ADDITIVE
            ? reconstruct_grains_additive_stream(std::any(sorted), ctx)
            : reconstruct_grains_stream(std::any(sorted), ctx);

        auto result = safe_any_cast_or_throw<
            Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(result_any);

        return std::dynamic_pointer_cast<Kakshya::DynamicSoundStream>(result.data);
    }

} // namespace

const ComputationGrammar::Rule::Executor reconstruct_grains =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    return reconstruct_with("reconstruct_grains", input, ctx, false, to_container);
};

const ComputationGrammar::Rule::Executor reconstruct_grains_additive =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    return reconstruct_with("reconstruct_grains_additive", input, ctx, true, to_container);
};

const ComputationGrammar::Rule::Executor reconstruct_grains_stream =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    return reconstruct_with("reconstruct_grains_stream", input, ctx, false, to_stream);
};

const ComputationGrammar::Rule::Executor reconstruct_grains_additive_stream =
    [](const std::any& input, const ExecutionContext& ctx) -> std::any {
    return reconstruct_with("reconstruct_grains_additive_stream", input, ctx, true, to_stream);
};

// ============================================================================
// SegmentOp
// ============================================================================

Datum<Kakshya::RegionGroup> SegmentOp::extract_implementation(
    const Datum<Kakshya::RegionGroup>& input)
{
    const auto& cfg = config<SegmentConfig>();

    std::shared_ptr<Kakshya::SignalSourceContainer> container;
    if (input.container && *input.container) {
        container = *input.container;
    } else {
        container = cfg.container;
    }

    if (!container) {
        error<std::runtime_error>(
            Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "SegmentOp: no container available via Datum or config");
    }

    const auto grain_size = cfg.grain_size;
    const auto hop_size = cfg.hop_size;
    const auto channel = cfg.channel;

    const auto& structure = container->get_structure();

    if (channel >= structure.get_channel_count()) {
        error<std::runtime_error>(
            Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "SegmentOp: channel {} out of range (container has {})",
            channel, structure.get_channel_count());
    }

    const auto num_channels = structure.get_channel_count();
    const auto total = container->get_num_frames();

    Datum<Kakshya::RegionGroup> out { input.data, container };
    out.data.regions.clear();
    out.data.current_region_index = 0;
    out.data.active_indices.clear();

    for (uint64_t onset = 0; onset + grain_size <= total; onset += hop_size) {
        Kakshya::Region grain {
            std::vector<uint64_t> { onset, 0 },
            std::vector<uint64_t> { onset + grain_size - 1, num_channels - 1 }
        };
        out.data.regions.push_back(std::move(grain));
    }

    out.data.set(grain_size_setting, grain_size);
    out.data.set(hop_size_setting, hop_size);

    return out;
}

// ============================================================================
// AttributeOp — private helpers
// ============================================================================

double AttributeOp::compute_grain_attribute(
    const Kakshya::Region& grain,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    const ExecutionContext& ctx) const
{
    const auto& cfg = config<AttributeConfig>();

    if (cfg.executor) {
        const auto samples = Kakshya::extract_region_channel(grain, container, cfg.channel);
        return cfg.executor(std::span<const double>(samples.data(), samples.size()), ctx);
    }

    const auto grain_variants = container->get_region_data(grain);

    if (cfg.analyzer) {
        if (cfg.analysis_type == AnalysisType::FEATURE) {
            return extract_scalar_energy(
                std::static_pointer_cast<VariantEnergyAnalyzer<>>(cfg.analyzer)
                    ->analyze_energy(grain_variants),
                static_cast<EnergyMethod>(cfg.qualifier));
        }

        if (cfg.analysis_type == AnalysisType::STATISTICAL) {
            return extract_scalar_statistics(
                std::static_pointer_cast<VariantStatisticalAnalyzer<>>(cfg.analyzer)
                    ->analyze_statistics(grain_variants),
                static_cast<StatisticalMethod>(cfg.qualifier));
        }
        return 0.0;
    }

    if (cfg.analysis_type == AnalysisType::FEATURE) {
        return extract_scalar_energy(
            m_energy_analyzer.analyze_energy(grain_variants),
            static_cast<EnergyMethod>(cfg.qualifier));
    }

    if (cfg.analysis_type == AnalysisType::STATISTICAL) {
        return extract_scalar_statistics(
            m_stat_analyzer.analyze_statistics(grain_variants),
            static_cast<StatisticalMethod>(cfg.qualifier));
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

    const auto& cfg = config<AttributeConfig>();

    const std::shared_ptr<Kakshya::SignalSourceContainer> container
        = (input.container && *input.container) ? *input.container : cfg.container;

    if (!container) {
        error<std::runtime_error>(
            Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "AttributeOp: no container available via Datum or config");
    }

    ExecutionContext ctx;
    ctx.facets.set(cfg);

    Datum<Kakshya::RegionGroup> out { input.data, container };

    for (auto& grain : out.data.regions)
        grain.set(grain_feature, compute_grain_attribute(grain, container, ctx));

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
    GranularOutput output,
    GrainTaper taper)
{
    auto matrix = std::make_shared<GranularMatrix>();

    matrix->create_operation<SegmentOp>("segment");
    matrix->create_operation<AttributeOp>("attribute");
    matrix->create_operation<SortOp>("sort");

    matrix->get_grammar()->create_rule("segment") //
        .with_priority(100)
        .with_description("Fixed hop-size grain segmentation")
        .matches_type<Kakshya::RegionGroup>()
        .executes(segment_grains)
        .build();

    matrix->get_grammar()->create_rule("attribute") //
        .with_priority(75)
        .with_description("Per-grain feature attribution via analyzer or lambda")
        .matches_type<Kakshya::RegionGroup>()
        .executes(attribute_grains)
        .build();

    matrix->get_grammar()->create_rule("sort") //
        .with_priority(50)
        .with_description("Attribute-keyed grain sort, CPU or GPU")
        .matches_type<Kakshya::RegionGroup>()
        .executes(sort_grains)
        .build();

    if (output == GranularOutput::CONTAINER) {
        matrix->get_grammar()->create_rule("reconstruct") //
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
                patched.facets.set(taper_facet, t);
            }
            return reconstruct_grains_additive(in, patched);
        };

        matrix->get_grammar()->create_rule("reconstruct") //
            .with_priority(25)
            .with_description("Additive grain reconstruction into SoundFileContainer")
            .matches_type<Kakshya::RegionGroup>()
            .executes(ola_executor)
            .build();
    } else if (output == GranularOutput::STREAM) {
        matrix->get_grammar()->create_rule("reconstruct") //
            .with_priority(25)
            .with_description("Stitch sorted grains into a DynamicSoundStream")
            .matches_type<Kakshya::RegionGroup>()
            .executes(reconstruct_grains_stream)
            .build();
    } else if (output == GranularOutput::STREAM_ADDITIVE) {
        auto ola_executor =
            [t = std::move(taper)](const std::any& in, const ExecutionContext& ctx) -> std::any {
            ExecutionContext patched = ctx;
            if (t)
                patched.facets.set(taper_facet, t);
            return reconstruct_grains_additive_stream(in, patched);
        };

        matrix->get_grammar()->create_rule("reconstruct") //
            .with_priority(25)
            .with_description("Additive grain reconstruction into DynamicSoundStream")
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
    Qualifier qualifier)
{
    auto matrix = make_granular_matrix();

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
    auto matrix = make_granular_matrix();

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
    Qualifier qualifier,
    GranularOutput output)
{
    auto ctx = make_granular_context(config, analysis_type, qualifier);

    return run_to_container(container, ctx, output);
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

    return run_to_container(container, ctx, output);
}

// ============================================================================
// process_to_stream — AnalysisType path
// ============================================================================

std::shared_ptr<Kakshya::DynamicSoundStream> process_to_stream(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config,
    Qualifier qualifier,
    GranularOutput output)
{
    auto ctx = make_granular_context(config, analysis_type, qualifier);

    return run_to_stream(container, ctx, output);
}

// ============================================================================
// process_to_stream — AttributeExecutor path
// ============================================================================

std::shared_ptr<Kakshya::DynamicSoundStream> process_to_stream(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    const GranularConfig& config,
    GranularOutput output)
{
    auto ctx = make_granular_context(config, std::move(executor));

    return run_to_stream(container, ctx, output);
}

} // namespace MayaFlux::Yantra::Granular
