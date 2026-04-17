#pragma once

#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/ComputePipeline.hpp"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Sorters/StandardSorter.hpp"

#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

namespace MayaFlux::Kakshya {
class SoundFileContainer;
class DynamicSoundStream;
}

namespace MayaFlux::Yantra::Granular {

/**
 * @brief Datum specialisation for granular processing pipelines.
 */
using GranularDatum = Datum<Kakshya::RegionGroup>;

/**
 * @brief Datum specialisation for container-output granular pipelines.
 */
using GranularContainerDatum = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>;

/**
 * @enum GranularOutput
 * @brief Selects the terminal output type produced by make_granular_matrix.
 *
 * REGION_GROUP — pipeline terminates with a sorted RegionGroup (default).
 *               Use when you need to inspect, analyse, or further transform
 *               the grain pool before committing to audio.
 *
 * CONTAINER    — appends a reconstruct rule that stitches sorted grains into
 *               a SoundFileContainer. Use for the offline playback workflow
 *               where container-in container-out is all that is needed.
 *
 * CONTAINER_ADDITIVE — overlap-add reconstruct: grains accumulated at hop_size
 *                      intervals with optional per-grain tapering.

 * STREAM    — appends a reconstruct rule that stitches sorted grains into
 *               a DynamicSoundStream. Use for sampling pipelines, StreamSlicing
 *                and similar real-time workflows
 *
 * STREAM_ADDITIVE — overlap-add reconstruct into a DynamicSoundStream.
 *                   grains accumulated at hop_size intervals with optional per-grain tapering.
 */
enum class GranularOutput : uint8_t {
    REGION_GROUP,
    CONTAINER,
    CONTAINER_ADDITIVE,
    STREAM,
    STREAM_ADDITIVE

};

/**
 * @brief Matrix type used throughout the granular subsystem.
 */
using GranularMatrix = GrammarAwareComputeMatrix;

/**
 * @brief Span-level escape hatch for fully custom per-grain feature computation.
 *
 * Receives the raw grain samples and the current ExecutionContext.
 * Returns a single double that is written as a named Region attribute.
 * For common attribution tasks prefer the AnalysisType + qualifier path
 * via make_granular_context.
 */
using AttributeExecutor = std::function<double(std::span<const double>, const ExecutionContext&)>;

/**
 * @brief Per-grain taper applied before accumulation in OLA reconstruction.
 *
 * Receives a mutable span covering exactly one grain's samples for one channel.
 * Applied in-place before the grain is added to the output buffer.
 * Pass {} or nullptr to skip tapering entirely.
 *
 * Common use:
 *   [](std::span<double> g){ Kinesis::Discrete::apply_hann(g); }
 *   [](std::span<double> g){ Kinesis::Discrete::apply_trapezoid(g, g.size() / 8); }
 */
using GrainTaper = Kakshya::RegionTaper;

/// @brief Grammar rule executor for the reconstruction step.
extern const ComputationGrammar::Rule::Executor reconstruct_grains;

/// @brief Grammar rule executor for additive grain reconstruction.
extern const ComputationGrammar::Rule::Executor reconstruct_grains_additive;

/// @brief Grammar rule executor — concatenative reconstruct into DynamicSoundStream.
extern const ComputationGrammar::Rule::Executor reconstruct_grains_stream;

/// @brief Grammar rule executor — OLA reconstruct into DynamicSoundStream.
extern const ComputationGrammar::Rule::Executor reconstruct_grains_additive_stream;

// ============================================================================
// Concrete operations
// ============================================================================

/**
 * @class SegmentOp
 * @brief Segments container audio into fixed hop-size grain windows.
 *
 * Reads parameters from set_parameter or ExecutionContext:
 * - grain_size  uint32_t  samples per grain    (default 1024)
 * - hop_size    uint32_t  hop between grains   (default 512)
 * - channel     uint32_t  source channel index (default 0)
 */
class MAYAFLUX_API SegmentOp final
    : public FeatureExtractor<Kakshya::RegionGroup, Kakshya::RegionGroup> {
public:
    SegmentOp()
        : FeatureExtractor<Kakshya::RegionGroup, Kakshya::RegionGroup>(
              1024, 512, ExtractionMethod::OVERLAPPING_WINDOWS)
    {
    }

    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::FEATURE_GUIDED;
    }

protected:
    [[nodiscard]] std::string get_extractor_name() const override
    {
        return "Granular::SegmentOp";
    }

    Datum<Kakshya::RegionGroup> extract_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;
};

/**
 * @class AttributeOp
 * @brief Computes a scalar attribute per grain and writes it onto each Region.
 *
 * Attribution is resolved in the following priority order:
 *
 * 1. Span lambda : set via set_parameter("attribute_executor", AttributeExecutor)
 *    or present in ExecutionContext. For fully custom math.
 *
 * 2. Direct analyzer : set via set_parameter("analyzer", shared_ptr<UniversalAnalyzer<...>>)
 *    paired with set_parameter("analyzer_qualifier", std::string).
 *    Any existing concrete analyzer can be supplied pre-configured.
 *
 * 3. Analysis type : set via set_parameter("analysis_type", AnalysisType) with
 *    optional set_parameter("analyzer_qualifier", std::string). AttributeOp
 *    constructs a default-configured analyzer internally.
 *
 * Parameters common to all paths:
 * - feature_key  std::string  attribute name written on each grain  (default "feature")
 * - channel      uint32_t     source channel index                  (default 0)
 *
 * Qualifier strings map directly to scalar fields of the analysis result struct.
 * The alias "rms" is accepted for FEATURE and resolves to "mean_energy".
 * The alias "mean" is accepted for STATISTICAL and resolves to "mean_stat".
 *
 * FEATURE qualifiers (EnergyAnalysis):
 * - "mean_energy"  (default, alias: "rms")
 * - "max_energy"
 * - "min_energy"
 * - "variance"
 * - "dynamic_range"
 * - "event_count"
 * - "window_count"
 *
 * STATISTICAL qualifiers (StatisticalAnalysis):
 * - "mean_stat"  (default, alias: "mean")
 * - "max_stat"
 * - "min_stat"
 * - "variance"
 * - "std_dev"
 * - "skewness"
 * - "kurtosis"
 * - "median"
 * - "window_count"
 */
class MAYAFLUX_API AttributeOp final
    : public RegionGroupAnalyzer<Kakshya::RegionGroup> {
public:
    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::FEATURE;
    }

protected:
    [[nodiscard]] std::string get_analyzer_name() const override
    {
        return "Granular::AttributeOp";
    }

    Datum<Kakshya::RegionGroup> analyze_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;

private:
    /**
     * @brief Resolve and execute attribution for a single grain.
     * @param grain Source region defining the grain boundaries.
     * @param container Signal data source.
     * @param channel Channel index to extract samples from.
     * @param ctx Current execution context carrying parameters.
     * @return Scalar attribute value for this grain.
     */
    double compute_grain_attribute(
        const Kakshya::Region& grain,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        uint32_t channel,
        const ExecutionContext& ctx) const;

    /**
     * @brief Construct a default-configured analyzer for the given AnalysisType.
     * @param type Analysis category.
     * @return Heap-allocated analyzer with sensible defaults.
     */
    static std::shared_ptr<RegionGroupAnalyzer<Kakshya::RegionGroup>>
    make_default_analyzer(AnalysisType type);

    /**
     * @brief Extract a named scalar from a typed analysis result.
     * @param analysis_result std::any holding the concrete analysis struct.
     * @param type Analysis category used to select the correct cast path.
     * @param qualifier Names the specific scalar to extract.
     * @return Extracted double value.
     */
    static double extract_scalar(const std::any& analysis_result,
        AnalysisType type,
        const std::string& qualifier);

    mutable VariantEnergyAnalyzer<> m_energy_analyzer;
    mutable VariantStatisticalAnalyzer<> m_stat_analyzer;
};

/**
 * @class SortOp
 * @brief Sorts grains by a named attribute key, with optional GPU dispatch.
 *
 * Parameters (via set_parameter or ExecutionContext):
 * - feature_key         std::string  attribute to sort on
 * - ascending           bool         sort direction                    (default true)
 * - gpu_sort_threshold  uint32_t     grain count threshold for GPU path (default 0 = CPU only)
 */
class MAYAFLUX_API SortOp final
    : public StandardSorter<Kakshya::RegionGroup, Kakshya::RegionGroup> {
public:
    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::ALGORITHMIC;
    }

protected:
    [[nodiscard]] std::string get_sorter_name() const override
    {
        return "Granular::SortOp";
    }

    Datum<Kakshya::RegionGroup> sort_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;
};

// ============================================================================
// Grammar rule executors
// ============================================================================

/// @brief Grammar rule executor for the segmentation step.
extern const ComputationGrammar::Rule::Executor segment_grains;

/// @brief Grammar rule executor for the attribution step.
extern const ComputationGrammar::Rule::Executor attribute_grains;

/// @brief Grammar rule executor for the sort step.
extern const ComputationGrammar::Rule::Executor sort_grains;

// ============================================================================
// Context construction
// ============================================================================

/**
 * @struct GranularConfig
 * @brief Scalar parameters shared across all granular pipeline entry points.
 *
 * Replaces the repeated positional argument lists on process, process_to_container,
 * and their async variants. Attribution type and output mode are kept as explicit
 * parameters since they affect overload resolution and return type respectively.
 */
struct GranularConfig {
    uint32_t grain_size = 1024;
    uint32_t hop_size = 512;
    std::string feature_key = "feature";
    uint32_t channel = 0;
    bool ascending = true;
    uint32_t gpu_sort_threshold = 0;
    ComputationContext attribution_context = ComputationContext::SPECTRAL;
    GrainTaper taper;
};

/**
 * @brief Construct an ExecutionContext for the granular pipeline using a built-in AnalysisType.
 *
 * @param config             Pipeline scalar parameters.
 * @param analysis_type      Attribution category. Supported: FEATURE, STATISTICAL.
 * @param qualifier          Scalar to extract. Empty uses type default.
 * @return Populated ExecutionContext.
 */
[[nodiscard]] inline ExecutionContext make_granular_context(
    const GranularConfig& config,
    AnalysisType analysis_type,
    const std::string& qualifier = {})
{
    ExecutionContext ctx;
    ctx.execution_metadata["grain_size"] = config.grain_size;
    ctx.execution_metadata["hop_size"] = config.hop_size;
    ctx.execution_metadata["channel"] = config.channel;
    ctx.execution_metadata["feature_key"] = config.feature_key;
    ctx.execution_metadata["ascending"] = config.ascending;
    ctx.execution_metadata["gpu_sort_threshold"] = config.gpu_sort_threshold;
    ctx.execution_metadata["analysis_type"] = analysis_type;
    ctx.execution_metadata["analyzer_qualifier"] = qualifier;
    return ctx;
}

/**
 * @brief Construct an ExecutionContext supplying a pre-configured analyzer instance directly.
 *
 * @tparam InputType  Analyzer input data type.
 * @tparam OutputType Analyzer output data type.
 * @param config    Pipeline scalar parameters.
 * @param analyzer  Pre-configured analyzer instance.
 * @param qualifier Scalar to extract. Empty uses type default.
 * @return Populated ExecutionContext.
 */
template <ComputeData InputType, ComputeData OutputType>
[[nodiscard]] inline ExecutionContext make_granular_context(
    const GranularConfig& config,
    std::shared_ptr<UniversalAnalyzer<InputType, OutputType>> analyzer,
    const std::string& qualifier = {})
{
    ExecutionContext ctx;
    ctx.execution_metadata["grain_size"] = config.grain_size;
    ctx.execution_metadata["hop_size"] = config.hop_size;
    ctx.execution_metadata["channel"] = config.channel;
    ctx.execution_metadata["feature_key"] = config.feature_key;
    ctx.execution_metadata["ascending"] = config.ascending;
    ctx.execution_metadata["gpu_sort_threshold"] = config.gpu_sort_threshold;
    ctx.execution_metadata["analyzer"] = std::static_pointer_cast<void>(analyzer);
    ctx.execution_metadata["analyzer_qualifier"] = qualifier;
    return ctx;
}

/**
 * @brief Construct an ExecutionContext using a span-level lambda for fully custom attribution.
 *
 * @param config   Pipeline scalar parameters.
 * @param executor Lambda receiving grain samples and context, returning a scalar.
 * @return Populated ExecutionContext.
 */
[[nodiscard]] inline ExecutionContext make_granular_context(
    const GranularConfig& config,
    AttributeExecutor executor)
{
    ExecutionContext ctx;
    ctx.execution_metadata["grain_size"] = config.grain_size;
    ctx.execution_metadata["hop_size"] = config.hop_size;
    ctx.execution_metadata["channel"] = config.channel;
    ctx.execution_metadata["feature_key"] = config.feature_key;
    ctx.execution_metadata["ascending"] = config.ascending;
    ctx.execution_metadata["gpu_sort_threshold"] = config.gpu_sort_threshold;
    ctx.execution_metadata["attribute_executor"] = std::move(executor);
    return ctx;
}

// ============================================================================
// Input construction
// ============================================================================

/**
 * @brief Construct the initial GranularDatum from a container.
 * @param container Source signal data.
 * @param group_name Name assigned to the RegionGroup. Defaults to "grains".
 * @return GranularDatum ready for pipeline input.
 */
[[nodiscard]] inline GranularDatum make_granular_input(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    const std::string& group_name = "grains")
{
    return GranularDatum { Kakshya::RegionGroup { group_name }, container };
}

// ============================================================================
// Matrix factory
// ============================================================================

/**
 * @brief Construct a GranularMatrix with grammar rules appropriate for the
 *        requested output type.
 *
 * REGION_GROUP: registers segment, attribute, sort (existing behaviour).
 * CONTAINER: registers segment, attribute, sort, then appends reconstruct.
 *            The reconstruct rule reads the source container from
 *            ExecutionContext::execution_metadata["container"] and writes
 *            stitched per-channel audio into a SoundFileContainer.
 * CONTAINER_ADDITIVE: same as CONTAINER but uses an overlap-add reconstruct rule
 *
 * @param attribution_context ComputationContext assigned to the attribution rule.
 * @param output              Terminal output type for this pipeline.
 * @param taper               Optional per-grain taper for OLA output.
 * @return Configured GranularMatrix instance.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<GranularMatrix> make_granular_matrix(
    ComputationContext attribution_context = ComputationContext::SPECTRAL,
    GranularOutput output = GranularOutput::REGION_GROUP, GrainTaper taper = {});

// ============================================================================
// Free process function
// ============================================================================

/**
 * @brief Run segment -> attribute -> sort and return the attributed RegionGroup.
 *
 * @param container     Source signal data.
 * @param analysis_type Attribution category.
 * @param config        Pipeline scalar parameters.
 * @param qualifier     Scalar to extract. Empty uses type default.
 * @return GranularDatum containing the attributed, sorted RegionGroup.
 */
[[nodiscard]] MAYAFLUX_API GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config = {},
    const std::string& qualifier = {});

/**
 * @brief Run segment -> attribute -> sort using a span-level attribution lambda.
 *
 * @param container Source signal data.
 * @param executor  Lambda receiving grain samples and context, returning a scalar.
 * @param config    Pipeline scalar parameters.
 * @return GranularDatum containing the attributed, sorted RegionGroup.
 */
[[nodiscard]] MAYAFLUX_API GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    const GranularConfig& config = {});

/**
 * @brief Offline granular pipeline terminating in a SoundFileContainer.
 *
 * @param container     Source signal data.
 * @param analysis_type Attribution category.
 * @param config        Pipeline scalar parameters.
 * @param qualifier     Scalar to extract. Empty uses type default.
 * @param output        CONTAINER for concatenative, CONTAINER_ADDITIVE for OLA.
 * @return Populated SoundFileContainer.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SoundFileContainer> process_to_container(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config = {},
    const std::string& qualifier = {},
    GranularOutput output = GranularOutput::CONTAINER);

/**
 * @brief Offline granular pipeline using a span-level attribution lambda.
 *
 * @param container Source signal data.
 * @param executor  Lambda receiving grain samples and context, returning a scalar.
 * @param config    Pipeline scalar parameters.
 * @param output    CONTAINER for concatenative, CONTAINER_ADDITIVE for OLA.
 * @return Populated SoundFileContainer.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SoundFileContainer> process_to_container(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    const GranularConfig& config = {},
    GranularOutput output = GranularOutput::CONTAINER);

/**
 * @brief Offline granular pipeline terminating in a DynamicSoundStream.
 *
 * Reconstructed grains are written channel-by-channel via
 * DynamicSoundStream::write_frames. The result is ready for direct use
 * as a SamplingPipeline source.
 *
 * @param container     Source signal data.
 * @param analysis_type Attribution category.
 * @param config        Pipeline scalar parameters.
 * @param qualifier     Scalar to extract. Empty uses type default.
 * @param output        STREAM for concatenative, STREAM_ADDITIVE for OLA.
 * @return Populated DynamicSoundStream.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::DynamicSoundStream> process_to_stream(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config = {},
    const std::string& qualifier = {},
    GranularOutput output = GranularOutput::STREAM);

/**
 * @brief Offline granular pipeline using a span-level attribution lambda,
 *        terminating in a DynamicSoundStream.
 *
 * @param container Source signal data.
 * @param executor  Lambda receiving grain samples and context, returning a scalar.
 * @param config    Pipeline scalar parameters.
 * @param output    STREAM for concatenative, STREAM_ADDITIVE for OLA.
 * @return Populated DynamicSoundStream.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::DynamicSoundStream> process_to_stream(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    const GranularConfig& config = {},
    GranularOutput output = GranularOutput::STREAM);

/**
 * @brief Async offline granular pipeline (AnalysisType path).
 *
 * Runs the full pipeline on a background thread owned by @p matrix and
 * invokes @p on_complete with the result. Returns immediately.
 *
 * @param matrix        ComputeMatrix that owns the async future.
 * @param container     Source signal data.
 * @param analysis_type Attribution category.
 * @param on_complete   Called on the worker thread with the finished container.
 * @param config        Pipeline scalar parameters.
 * @param qualifier     Scalar to extract. Empty uses type default.
 * @param output        CONTAINER for concatenative, CONTAINER_ADDITIVE for OLA.
 */
template <typename CompleteFn>
void process_to_container_async(
    const std::shared_ptr<GranularMatrix>& matrix,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    CompleteFn&& on_complete,
    const GranularConfig& config = {},
    const std::string& qualifier = {},
    GranularOutput output = GranularOutput::CONTAINER)
{
    auto ctx = make_granular_context(config, analysis_type, qualifier);

    ctx.execution_metadata["container"] = container;
    if (config.taper)
        ctx.execution_metadata["grain_taper"] = config.taper;

    auto seg_op = matrix->get_operation<SegmentOp>("segment");
    auto attr_op = matrix->get_operation<AttributeOp>("attribute");
    auto sort_op = matrix->get_operation<SortOp>("sort");
    apply_context_parameters(seg_op, ctx);
    apply_context_parameters(attr_op, ctx);
    apply_context_parameters(sort_op, ctx);

    matrix->with_async(make_granular_input(container), [seg_op, attr_op, sort_op, ctx, output](auto chain) {
            auto sorted = chain
                .template then<SegmentOp>("segment")
                .template then<AttributeOp>("attribute")
                .template then<SortOp>("sort")
                .to_io();
            if (output == GranularOutput::CONTAINER_ADDITIVE) {
                return safe_any_cast_or_throw<
                    Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                        reconstruct_grains_additive(std::any(sorted), ctx));
            }
            return safe_any_cast_or_throw<
                Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                    reconstruct_grains(std::any(sorted), ctx)); }, std::forward<CompleteFn>(on_complete));
}

/**
 * @brief Async offline granular pipeline (AttributeExecutor path).
 *
 * @param matrix      ComputeMatrix that owns the async future.
 * @param container   Source signal data.
 * @param executor    Lambda receiving grain samples and context, returning a scalar.
 * @param on_complete Called on the worker thread with the finished container.
 * @param config      Pipeline scalar parameters.
 * @param output      CONTAINER for concatenative, CONTAINER_ADDITIVE for OLA.
 */
template <typename CompleteFn>
void process_to_container_async(
    const std::shared_ptr<GranularMatrix>& matrix,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    CompleteFn&& on_complete,
    const GranularConfig& config = {},
    GranularOutput output = GranularOutput::CONTAINER)
{
    auto ctx = make_granular_context(config, std::move(executor));

    ctx.execution_metadata["container"] = container;
    if (config.taper)
        ctx.execution_metadata["grain_taper"] = config.taper;

    auto seg_op = matrix->get_operation<SegmentOp>("segment");
    auto attr_op = matrix->get_operation<AttributeOp>("attribute");
    auto sort_op = matrix->get_operation<SortOp>("sort");
    apply_context_parameters(seg_op, ctx);
    apply_context_parameters(attr_op, ctx);
    apply_context_parameters(sort_op, ctx);

    matrix->with_async(make_granular_input(container), [seg_op, attr_op, sort_op, ctx, output](auto chain) {
            auto sorted = chain
                .template then<SegmentOp>("segment")
                .template then<AttributeOp>("attribute")
                .template then<SortOp>("sort")
                .to_io();
            if (output == GranularOutput::CONTAINER_ADDITIVE) {
                return safe_any_cast_or_throw<
                    Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                        reconstruct_grains_additive(std::any(sorted), ctx));
            }
            return safe_any_cast_or_throw<
                Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                    reconstruct_grains(std::any(sorted), ctx)); }, std::forward<CompleteFn>(on_complete));
}

/**
 * @brief Async offline granular pipeline terminating in a DynamicSoundStream
 *        (AnalysisType path).
 *
 * Runs the full pipeline on a background thread owned by @p matrix and
 * invokes @p on_complete with the finished stream. Returns immediately.
 *
 * The callback receives a @c shared_ptr<DynamicSoundStream> which is null
 * if reconstruction produced an unexpected type. The caller is responsible
 * for thread-safe installation of the stream into any downstream pipeline.
 *
 * @param matrix        ComputeMatrix that owns the async future.
 * @param container     Source signal data.
 * @param analysis_type Attribution category.
 * @param on_complete   Called on the worker thread with the finished stream.
 * @param config        Pipeline scalar parameters.
 * @param qualifier     Scalar to extract. Empty uses type default.
 * @param output        STREAM for concatenative, STREAM_ADDITIVE for OLA.
 */
template <typename CompleteFn>
void process_to_stream_async(
    const std::shared_ptr<GranularMatrix>& matrix,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    CompleteFn&& on_complete,
    const GranularConfig& config = {},
    const std::string& qualifier = {},
    GranularOutput output = GranularOutput::STREAM)
{
    auto ctx = make_granular_context(config, analysis_type, qualifier);
    ctx.execution_metadata["container"] = container;
    if (config.taper)
        ctx.execution_metadata["grain_taper"] = config.taper;

    auto seg_op = matrix->get_operation<SegmentOp>("segment");
    auto attr_op = matrix->get_operation<AttributeOp>("attribute");
    auto sort_op = matrix->get_operation<SortOp>("sort");
    apply_context_parameters(seg_op, ctx);
    apply_context_parameters(attr_op, ctx);
    apply_context_parameters(sort_op, ctx);

    matrix->with_async(
        make_granular_input(container),
        [ctx, output](auto chain) {
            auto sorted = chain
                              .template then<SegmentOp>("segment")
                              .template then<AttributeOp>("attribute")
                              .template then<SortOp>("sort")
                              .to_io();
            if (output == GranularOutput::STREAM_ADDITIVE) {
                return safe_any_cast_or_throw<
                    Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                    reconstruct_grains_additive_stream(std::any(sorted), ctx));
            }
            return safe_any_cast_or_throw<
                Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                reconstruct_grains_stream(std::any(sorted), ctx));
        },
        [on_complete = std::forward<CompleteFn>(on_complete)](
            const Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>& result) {
            on_complete(std::dynamic_pointer_cast<Kakshya::DynamicSoundStream>(result.data));
        });
}

/**
 * @brief Async offline granular pipeline terminating in a DynamicSoundStream
 *        (AttributeExecutor path).
 *
 * @param matrix      ComputeMatrix that owns the async future.
 * @param container   Source signal data.
 * @param executor    Lambda receiving grain samples and context, returning a scalar.
 * @param on_complete Called on the worker thread with the finished stream.
 * @param config      Pipeline scalar parameters.
 * @param output      STREAM for concatenative, STREAM_ADDITIVE for OLA.
 */
template <typename CompleteFn>
void process_to_stream_async(
    const std::shared_ptr<GranularMatrix>& matrix,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AttributeExecutor executor,
    CompleteFn&& on_complete,
    const GranularConfig& config = {},
    GranularOutput output = GranularOutput::STREAM)
{
    auto ctx = make_granular_context(config, std::move(executor));
    ctx.execution_metadata["container"] = container;
    if (config.taper)
        ctx.execution_metadata["grain_taper"] = config.taper;

    auto seg_op = matrix->get_operation<SegmentOp>("segment");
    auto attr_op = matrix->get_operation<AttributeOp>("attribute");
    auto sort_op = matrix->get_operation<SortOp>("sort");
    apply_context_parameters(seg_op, ctx);
    apply_context_parameters(attr_op, ctx);
    apply_context_parameters(sort_op, ctx);

    matrix->with_async(
        make_granular_input(container),
        [ctx, output](auto chain) {
            auto sorted = chain
                              .template then<SegmentOp>("segment")
                              .template then<AttributeOp>("attribute")
                              .template then<SortOp>("sort")
                              .to_io();
            if (output == GranularOutput::STREAM_ADDITIVE) {
                return safe_any_cast_or_throw<
                    Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                    reconstruct_grains_additive_stream(std::any(sorted), ctx));
            }
            return safe_any_cast_or_throw<
                Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                reconstruct_grains_stream(std::any(sorted), ctx));
        },
        [on_complete = std::forward<CompleteFn>(on_complete)](
            const Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>& result) {
            on_complete(std::dynamic_pointer_cast<Kakshya::DynamicSoundStream>(result.data));
        });
}

} // namespace MayaFlux::Yantra::Granular
