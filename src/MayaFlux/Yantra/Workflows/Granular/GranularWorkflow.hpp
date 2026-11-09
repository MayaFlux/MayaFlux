#pragma once

/**
 * @file GranularWorkflow.hpp
 * @brief Compatibility granular workflow on typed stages. The rebuilt workflow is written beside it.
 */

#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/ComputePipeline.hpp"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Sorters/RegionKeySorter.hpp"

#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

namespace MayaFlux::Kakshya {
class SoundFileContainer;
class DynamicSoundStream;
}

namespace MayaFlux::Yantra::Granular {

/**
 * @brief Summary value selector for an analysis.
 *
 * Holds an EnergyMethod value for AnalysisType::FEATURE and a StatisticalMethod
 * value for AnalysisType::STATISTICAL. Zero selects the family's first method
 * (RMS or MEAN).
 */
using Qualifier = uint8_t;

/**
 * @brief The computed feature value of one grain, stored on the region.
 *
 * AttributeOp writes it and the sorter orders regions by it through a sort key.
 */
inline const Facet<double> grain_feature {};

/** @brief A second per-grain value, for granular pipelines that measure two features. */
inline const Facet<double> grain_secondary_feature {};

/** @brief Grain length in samples, stored once per group. */
inline const Facet<uint32_t> grain_size_setting {};
/** @brief Hop between grain onsets in samples, stored once per group. */
inline const Facet<uint32_t> hop_size_setting {};

/**
 * @brief Datum specialisation for granular processing pipelines.
 */
using GranularDatum = Datum<Kakshya::RegionGroup>;

/**
 * @enum GranularOutput
 * @brief Selects the terminal output of a granular matrix.
 *
 * REGION_GROUP: the pipeline ends with a sorted RegionGroup (default).
 * CONTAINER: sorted grains are concatenated into a SoundFileContainer.
 * CONTAINER_ADDITIVE: grains are overlap-added at hop_size intervals into a SoundFileContainer.
 * STREAM: sorted grains are concatenated into a DynamicSoundStream.
 * STREAM_ADDITIVE: grains are overlap-added at hop_size intervals into a DynamicSoundStream.
 */
enum class GranularOutput : uint8_t {
    REGION_GROUP,
    CONTAINER,
    CONTAINER_ADDITIVE,
    STREAM,
    STREAM_ADDITIVE
};

/**
 * @brief Span-level escape hatch for fully custom per-grain feature computation.
 *
 * Receives the raw grain samples and the current ExecutionContext.
 * Returns a single double that is written to the region's grain_feature facet.
 */
using AttributeExecutor = std::function<double(std::span<const double>, const ExecutionContext&)>;

/**
 * @brief Per-grain taper applied before accumulation in overlap-add reconstruction.
 *
 * Receives a mutable span covering exactly one grain's samples for one channel.
 * Pass {} or nullptr to skip tapering entirely.
 */
using GrainTaper = Kakshya::RegionTaper;

/// @brief Grammar rule executor for the reconstruction step.
extern MAYAFLUX_API const ComputationGrammar::Rule::Executor reconstruct_grains;

/// @brief Grammar rule executor for additive grain reconstruction.
extern MAYAFLUX_API const ComputationGrammar::Rule::Executor reconstruct_grains_additive;

/// @brief Grammar rule executor for concatenative reconstruction into DynamicSoundStream.
extern MAYAFLUX_API const ComputationGrammar::Rule::Executor reconstruct_grains_stream;

/// @brief Grammar rule executor for overlap-add reconstruction into DynamicSoundStream.
extern MAYAFLUX_API const ComputationGrammar::Rule::Executor reconstruct_grains_additive_stream;

/**
 * @enum GranularStage
 * @brief The stages of a granular pipeline, as grammar selections.
 */
enum class GranularStage : uint8_t {
    Segment,
    Attribute,
    Sort,
    Reconstruct
};

// ============================================================================
// Concrete operations
// ============================================================================

/**
 * @struct SegmentConfig
 * @brief Settings of SegmentOp, stored whole in the operation's facets.
 */
struct SegmentConfig : OperationConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&SegmentConfig::grain_size> {},
            FacetMember<&SegmentConfig::hop_size> {},
            FacetMember<&SegmentConfig::channel> {},
            FacetMember<&SegmentConfig::container> {},
        };
    }

    /** Samples per grain. */
    uint32_t grain_size { 1024 };
    /** Samples between successive grain onsets. */
    uint32_t hop_size { 512 };
    /** Source channel index. */
    uint32_t channel { 0 };
    /** Source used when the Datum carries no container. */
    std::shared_ptr<Kakshya::SignalSourceContainer> container;
};

/**
 * @class SegmentOp
 * @brief Segments container audio into fixed hop-size grain windows.
 */
class MAYAFLUX_API SegmentOp final
    : public Extractor<Kakshya::RegionGroup, Kakshya::RegionGroup> {
public:
    using supports = Supports<SegmentConfig>;

    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::FEATURE_GUIDED;
    }

    /**
     * @brief The stored settings, or defaults when none were set.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return stored_configs(supports {});
    }

protected:
    Datum<Kakshya::RegionGroup> extract_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;
};

/**
 * @struct AttributeConfig
 * @brief Settings of AttributeOp, stored whole in the operation's facets.
 *
 * Attribution is resolved in the following priority order:
 *
 * 1. executor : a span lambda for fully custom math.
 * 2. analyzer : a pre-configured analyzer, paired with qualifier.
 * 3. analysis_type : AttributeOp uses a default-configured analyzer of that family.
 */
struct AttributeConfig : OperationConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&AttributeConfig::channel> {},
            FacetMember<&AttributeConfig::analysis_type> {},
            FacetMember<&AttributeConfig::qualifier> {},
            FacetMember<&AttributeConfig::analyzer> {},
            FacetMember<&AttributeConfig::executor> {},
            FacetMember<&AttributeConfig::container> {},
        };
    }

    /** Source channel index. */
    uint32_t channel { 0 };
    /** Analyzer family used when no executor is set. */
    AnalysisType analysis_type { AnalysisType::FEATURE };
    /** Scalar selected from the analysis result. */
    Qualifier qualifier { 0 };
    /** Pre-configured analyzer of the family named by analysis_type. */
    std::shared_ptr<void> analyzer;
    /** Custom per-grain math. */
    AttributeExecutor executor;
    /** Source used when the Datum carries no container. */
    std::shared_ptr<Kakshya::SignalSourceContainer> container;
};

/**
 * @class AttributeOp
 * @brief Computes a scalar attribute per grain and writes it onto each Region.
 *
 * The per-grain value is stored in the grain_feature facet of each region.
 */
class MAYAFLUX_API AttributeOp final
    : public RegionGroupAnalyzer<Kakshya::RegionGroup> {
public:
    using supports = Supports<AttributeConfig>;

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return config<AttributeConfig>().analysis_type;
    }

    /**
     * @brief The stored settings, or defaults when none were set.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return stored_configs(supports {});
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "Granular::AttributeOp";
    }

protected:
    Datum<Kakshya::RegionGroup> analyze_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;

private:
    /**
     * @brief Resolve and execute attribution for a single grain.
     * @param grain Source region defining the grain boundaries.
     * @param container Signal data source.
     * @param ctx Execution context passed to a custom executor.
     * @return Scalar attribute value for this grain.
     */
    double compute_grain_attribute(
        const Kakshya::Region& grain,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        const ExecutionContext& ctx) const;

    mutable VariantEnergyAnalyzer<> m_energy_analyzer;
    mutable VariantStatisticalAnalyzer<> m_stat_analyzer;
};

/**
 * @class GrainReconstructOp
 * @brief Stitches sorted grains into a container or a stream.
 *
 * Built with the output selection and, for the additive outputs, the taper applied
 * to each grain before it is accumulated.
 */
class MAYAFLUX_API GrainReconstructOp final
    : public ComputeOperation<Kakshya::RegionGroup, std::shared_ptr<Kakshya::SignalSourceContainer>> {
public:
    /**
     * @brief Construct a reconstruction operation.
     * @param output Concatenative or additive, into a container or a stream. REGION_GROUP is rejected.
     * @param taper Per-grain taper for the additive outputs. Empty skips tapering.
     */
    explicit GrainReconstructOp(GranularOutput output, GrainTaper taper = {});

    [[nodiscard]] OperationType get_operation_type() const override
    {
        return OperationType::CUSTOM;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "Granular::GrainReconstructOp";
    }

protected:
    Datum<std::shared_ptr<Kakshya::SignalSourceContainer>> operation_function(
        const Datum<Kakshya::RegionGroup>& input) override;

private:
    GranularOutput m_output;
    GrainTaper m_taper;
};

/**
 * @class GranularMatrix
 * @brief The granular pipeline as a compute matrix with typed stages.
 *
 * Built once per pipeline. Each stage is a typed member, reached through segment(),
 * attribute() and sort(), and the grammar holds the same operations under
 * GranularStage selections. Rules added to the grammar by name still override stages
 * through execute_with_grammar().
 */
class MAYAFLUX_API GranularMatrix : public GrammarAwareComputeMatrix {
public:
    /**
     * @brief Build the stages and their grammar for an output.
     * @param output Terminal output. REGION_GROUP leaves out the reconstruct stage.
     * @param taper Per-grain taper for the additive outputs.
     */
    explicit GranularMatrix(GranularOutput output = GranularOutput::REGION_GROUP, GrainTaper taper = {});

    [[nodiscard]] std::shared_ptr<SegmentOp> segment() const { return m_segment; }
    [[nodiscard]] std::shared_ptr<AttributeOp> attribute() const { return m_attribute; }
    [[nodiscard]] std::shared_ptr<RegionKeySorter> sort() const { return m_sort; }

private:
    std::shared_ptr<SegmentOp> m_segment;
    std::shared_ptr<AttributeOp> m_attribute;
    std::shared_ptr<RegionKeySorter> m_sort;
};

// ============================================================================
// Grammar rule executors
// ============================================================================

/// @brief Grammar rule executor for the segmentation step.
extern MAYAFLUX_API const ComputationGrammar::Rule::Executor segment_grains;

/// @brief Grammar rule executor for the attribution step.
extern MAYAFLUX_API const ComputationGrammar::Rule::Executor attribute_grains;

/// @brief Grammar rule executor for the sort step.
extern MAYAFLUX_API const ComputationGrammar::Rule::Executor sort_grains;

// ============================================================================
// Context construction
// ============================================================================

/**
 * @struct GranularConfig
 * @brief Scalar parameters shared across all granular pipeline entry points.
 */
struct GranularConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&GranularConfig::grain_size> {},
            FacetMember<&GranularConfig::hop_size> {},
            FacetMember<&GranularConfig::channel> {},
            FacetMember<&GranularConfig::ascending> {},
            FacetMember<&GranularConfig::gpu_sort_threshold> {},
            FacetMember<&GranularConfig::taper> {},
        };
    }

    uint32_t grain_size = 1024;
    uint32_t hop_size = 512;
    uint32_t channel = 0;
    bool ascending = true;
    uint32_t gpu_sort_threshold = 0;
    GrainTaper taper;
};

/**
 * @brief Key under which a context carries the reconstruction taper.
 *
 * make_granular_context stores GranularConfig::taper here when it is set, and the
 * additive reconstruct executors read it.
 */
inline constexpr FacetMember<&GranularConfig::taper> taper_facet {};

namespace detail {

    /**
     * @brief The context part shared by every attribution path: segmentation, the sort key and the taper.
     */
    inline ExecutionContext granular_base_context(const GranularConfig& config)
    {
        ExecutionContext ctx;
        SegmentConfig segment;
        segment.grain_size = config.grain_size;
        segment.hop_size = config.hop_size;
        segment.channel = config.channel;
        ctx.facets.set(std::move(segment));
        RegionKeySorterConfig sort;
        sort.keys = { SortKey { grain_feature, config.ascending ? SortingDirection::ASCENDING : SortingDirection::DESCENDING, 1.0, false } };
        sort.gpu_sort_threshold = config.gpu_sort_threshold;
        ctx.facets.set(std::move(sort));
        if (config.taper) {
            ctx.facets.set(taper_facet, config.taper);
        }
        return ctx;
    }
}

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
    Qualifier qualifier = 0)
{
    auto ctx = detail::granular_base_context(config);
    AttributeConfig attribute;
    attribute.channel = config.channel;
    attribute.analysis_type = analysis_type;
    attribute.qualifier = qualifier;
    ctx.facets.set(std::move(attribute));
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
    std::shared_ptr<Analyzer<InputType, OutputType>> analyzer,
    Qualifier qualifier = 0)
{
    auto ctx = detail::granular_base_context(config);
    AttributeConfig attribute;
    attribute.channel = config.channel;
    attribute.analysis_type = analyzer->get_analysis_type();
    attribute.qualifier = qualifier;
    attribute.analyzer = std::static_pointer_cast<void>(analyzer);
    ctx.facets.set(std::move(attribute));
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
    auto ctx = detail::granular_base_context(config);
    AttributeConfig attribute;
    attribute.channel = config.channel;
    attribute.executor = std::move(executor);
    ctx.facets.set(std::move(attribute));
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
// Matrix factory and stage runners
// ============================================================================

/**
 * @brief Construct a GranularMatrix for the requested output.
 *
 * @param output Terminal output type for this pipeline.
 * @param taper  Optional per-grain taper for the additive outputs.
 * @return Configured GranularMatrix instance.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<GranularMatrix> make_granular_matrix(
    GranularOutput output = GranularOutput::REGION_GROUP, GrainTaper taper = {});

namespace detail {

    /** @brief Apply one context's settings to every stage of a matrix. */
    inline void apply_granular_context(const std::shared_ptr<GranularMatrix>& matrix, const ExecutionContext& ctx)
    {
        apply_context_parameters(matrix->segment(), ctx);
        apply_context_parameters(matrix->attribute(), ctx);
        apply_context_parameters(matrix->sort(), ctx);
    }

    /** @brief Run segment, attribute and sort on a datum. */
    inline GranularDatum run_grain_stages(const std::shared_ptr<GranularMatrix>& matrix, const GranularDatum& input)
    {
        auto segmented = matrix->segment()->apply_operation(input);
        auto attributed = matrix->attribute()->apply_operation(segmented);
        return matrix->sort()->apply_operation(attributed);
    }
}

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
    Qualifier qualifier = 0);

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
 * @param output        CONTAINER for concatenative, CONTAINER_ADDITIVE for overlap-add.
 * @return Populated SoundFileContainer.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SoundFileContainer> process_to_container(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config = {},
    Qualifier qualifier = 0,
    GranularOutput output = GranularOutput::CONTAINER);

/**
 * @brief Offline granular pipeline using a span-level attribution lambda.
 *
 * @param container Source signal data.
 * @param executor  Lambda receiving grain samples and context, returning a scalar.
 * @param config    Pipeline scalar parameters.
 * @param output    CONTAINER for concatenative, CONTAINER_ADDITIVE for overlap-add.
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
 * @param output        STREAM for concatenative, STREAM_ADDITIVE for overlap-add.
 * @return Populated DynamicSoundStream.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::DynamicSoundStream> process_to_stream(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    const GranularConfig& config = {},
    Qualifier qualifier = 0,
    GranularOutput output = GranularOutput::STREAM);

/**
 * @brief Offline granular pipeline using a span-level attribution lambda,
 *        terminating in a DynamicSoundStream.
 *
 * @param container Source signal data.
 * @param executor  Lambda receiving grain samples and context, returning a scalar.
 * @param config    Pipeline scalar parameters.
 * @param output    STREAM for concatenative, STREAM_ADDITIVE for overlap-add.
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
 * @param matrix        Matrix that owns the async future.
 * @param container     Source signal data.
 * @param analysis_type Attribution category.
 * @param on_complete   Called on the worker thread with the finished container.
 * @param config        Pipeline scalar parameters.
 * @param qualifier     Scalar to extract. Empty uses type default.
 * @param output        CONTAINER for concatenative, CONTAINER_ADDITIVE for overlap-add.
 */
template <typename CompleteFn>
void process_to_container_async(
    const std::shared_ptr<GranularMatrix>& matrix,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    CompleteFn&& on_complete,
    const GranularConfig& config = {},
    Qualifier qualifier = 0,
    GranularOutput output = GranularOutput::CONTAINER)
{
    auto ctx = make_granular_context(config, analysis_type, qualifier);
    detail::apply_granular_context(matrix, ctx);

    matrix->with_async(
        make_granular_input(container),
        [matrix, ctx, output](auto chain) {
            auto sorted = detail::run_grain_stages(matrix, chain.to_io());
            if (output == GranularOutput::CONTAINER_ADDITIVE) {
                return safe_any_cast_or_throw<
                    Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                    reconstruct_grains_additive(std::any(sorted), ctx));
            }
            return safe_any_cast_or_throw<
                Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                reconstruct_grains(std::any(sorted), ctx));
        },
        std::forward<CompleteFn>(on_complete));
}

/**
 * @brief Async offline granular pipeline (AttributeExecutor path).
 *
 * @param matrix      Matrix that owns the async future.
 * @param container   Source signal data.
 * @param executor    Lambda receiving grain samples and context, returning a scalar.
 * @param on_complete Called on the worker thread with the finished container.
 * @param config      Pipeline scalar parameters.
 * @param output      CONTAINER for concatenative, CONTAINER_ADDITIVE for overlap-add.
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
    detail::apply_granular_context(matrix, ctx);

    matrix->with_async(
        make_granular_input(container),
        [matrix, ctx, output](auto chain) {
            auto sorted = detail::run_grain_stages(matrix, chain.to_io());
            if (output == GranularOutput::CONTAINER_ADDITIVE) {
                return safe_any_cast_or_throw<
                    Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                    reconstruct_grains_additive(std::any(sorted), ctx));
            }
            return safe_any_cast_or_throw<
                Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>>(
                reconstruct_grains(std::any(sorted), ctx));
        },
        std::forward<CompleteFn>(on_complete));
}

/**
 * @brief Async offline granular pipeline terminating in a DynamicSoundStream
 *        (AnalysisType path).
 *
 * The callback receives a @c shared_ptr<DynamicSoundStream> which is null
 * if reconstruction produced an unexpected type. The caller is responsible
 * for thread-safe installation of the stream into any downstream pipeline.
 *
 * @param matrix        Matrix that owns the async future.
 * @param container     Source signal data.
 * @param analysis_type Attribution category.
 * @param on_complete   Called on the worker thread with the finished stream.
 * @param config        Pipeline scalar parameters.
 * @param qualifier     Scalar to extract. Empty uses type default.
 * @param output        STREAM for concatenative, STREAM_ADDITIVE for overlap-add.
 */
template <typename CompleteFn>
void process_to_stream_async(
    const std::shared_ptr<GranularMatrix>& matrix,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    AnalysisType analysis_type,
    CompleteFn&& on_complete,
    const GranularConfig& config = {},
    Qualifier qualifier = 0,
    GranularOutput output = GranularOutput::STREAM)
{
    auto ctx = make_granular_context(config, analysis_type, qualifier);
    detail::apply_granular_context(matrix, ctx);

    matrix->with_async(
        make_granular_input(container),
        [matrix, ctx, output](auto chain) {
            auto sorted = detail::run_grain_stages(matrix, chain.to_io());
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
 * @param matrix      Matrix that owns the async future.
 * @param container   Source signal data.
 * @param executor    Lambda receiving grain samples and context, returning a scalar.
 * @param on_complete Called on the worker thread with the finished stream.
 * @param config      Pipeline scalar parameters.
 * @param output      STREAM for concatenative, STREAM_ADDITIVE for overlap-add.
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
    detail::apply_granular_context(matrix, ctx);

    matrix->with_async(
        make_granular_input(container),
        [matrix, ctx, output](auto chain) {
            auto sorted = detail::run_grain_stages(matrix, chain.to_io());
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
